package com.arena.websocket;

import com.arena.entity.User;
import com.arena.repository.UserRepository;
import com.arena.service.MatchmakingService;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.socket.*;
import org.springframework.web.socket.handler.TextWebSocketHandler;

import java.io.IOException;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

@Component
public class GameWebSocketHandler extends TextWebSocketHandler {

    private static final Logger log = LoggerFactory.getLogger(GameWebSocketHandler.class);

    private final ObjectMapper mapper = new ObjectMapper();
    private final UserRepository users;
    private final MatchmakingService matchmaking;

    // arenaId -> live sessions in that arena
    private final Map<String, Set<WebSocketSession>> arenas = new ConcurrentHashMap<>();
    // sessionId -> the arena and player it belongs to
    private final Map<String, String> sessionArena = new ConcurrentHashMap<>();
    private final Map<String, String> sessionPlayer = new ConcurrentHashMap<>();

    public GameWebSocketHandler(UserRepository users, MatchmakingService matchmaking) {
        this.users = users;
        this.matchmaking = matchmaking;
    }

    @Override
    public void afterConnectionEstablished(WebSocketSession session) {
        String arenaId = arenaOf(session);
        arenas.computeIfAbsent(arenaId, k -> ConcurrentHashMap.newKeySet()).add(session);
        sessionArena.put(session.getId(), arenaId);
        log.info("session {} joined {} ({} in arena)",
                session.getId(), arenaId, arenas.get(arenaId).size());
    }

    @Override
    protected void handleTextMessage(WebSocketSession session, TextMessage message) {
        JsonNode node;
        try {
            node = mapper.readTree(message.getPayload());
        } catch (Exception e) {
            log.debug("dropping unparseable frame from {}", session.getId());
            return;
        }

        String type = node.path("type").asText("");
        String playerId = node.path("id").asText("");
        if (!playerId.isEmpty()) sessionPlayer.put(session.getId(), playerId);

        switch (type) {
            // Position, incoming fire and damage reports are all pure relay:
            // the arena simulation still lives on the clients.
            case "state", "shot", "hit" -> relay(session, message.getPayload());
            case "kill" -> {
                recordKill(playerId);
                relay(session, message.getPayload());
            }
            default -> log.debug("unknown frame type '{}'", type);
        }
    }

    @Override
    public void afterConnectionClosed(WebSocketSession session, CloseStatus status) {
        String arenaId = sessionArena.remove(session.getId());
        String playerId = sessionPlayer.remove(session.getId());

        if (arenaId != null) {
            Set<WebSocketSession> peers = arenas.get(arenaId);
            if (peers != null) {
                peers.remove(session);
                if (peers.isEmpty()) arenas.remove(arenaId);
            }
            if (playerId != null) {
                matchmaking.leaveArena(arenaId, playerId);
                // Tell the survivors to drop the ship, or it hangs in the arena.
                broadcast(arenaId, "{\"type\":\"leave\",\"id\":\"" + playerId + "\"}", session);
            }
        }
        log.info("session {} left ({})", session.getId(), status.getCode());
    }

    private void recordKill(String playerId) {
        // Client-reported, and therefore trusted only as far as a hobby arena
        // warrants. Making this authoritative means simulating bullets here
        // instead of relaying them -- noted in the README as the next step.
        users.findByUsername(playerId).ifPresent(u -> {
            u.setKills(u.getKills() + 1);
            u.setScore(u.getScore() + 10);
            users.save(u);
        });
    }

    private void relay(WebSocketSession from, String payload) {
        String arenaId = sessionArena.get(from.getId());
        if (arenaId != null) broadcast(arenaId, payload, from);
    }

    private void broadcast(String arenaId, String payload, WebSocketSession except) {
        Set<WebSocketSession> peers = arenas.get(arenaId);
        if (peers == null) return;
        TextMessage frame = new TextMessage(payload);
        for (WebSocketSession peer : peers) {
            if (peer.equals(except) || !peer.isOpen()) continue;
            try {
                // Spring's session is not safe for concurrent sends; two game
                // ticks landing at once would interleave frames without this.
                synchronized (peer) {
                    peer.sendMessage(frame);
                }
            } catch (IOException e) {
                log.warn("send to {} failed: {}", peer.getId(), e.getMessage());
            }
        }
    }

    private String arenaOf(WebSocketSession session) {
        var uri = session.getUri();
        if (uri == null || uri.getQuery() == null) return "arena-default";
        for (String pair : uri.getQuery().split("&")) {
            String[] kv = pair.split("=", 2);
            if (kv.length == 2 && kv[0].equals("arena")) return kv[1];
        }
        return "arena-default";
    }
}
