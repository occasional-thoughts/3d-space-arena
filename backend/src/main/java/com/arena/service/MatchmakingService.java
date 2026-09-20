package com.arena.service;

import com.arena.entity.Match;
import com.arena.repository.MatchRepository;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Assigns players to arenas.
 *
 * The original plan held players in a queue until four had gathered. That
 * deadlocks a game nobody is playing yet -- the first two people to show up
 * would wait forever for a third and fourth. Instead a player joins the
 * emptiest arena with a free slot and the match starts immediately; the arena
 * fills as more people arrive.
 */
@Service
public class MatchmakingService {

    public static final int MAX_PER_ARENA = 8;

    private final MatchRepository matches;
    private final Map<String, Set<String>> arenaMembers = new ConcurrentHashMap<>();

    @Value("${arena.public-ws-url:ws://localhost:8080/game}")
    private String publicWsUrl;

    public MatchmakingService(MatchRepository matches) {
        this.matches = matches;
    }

    public synchronized String joinArena(String username) {
        for (var entry : arenaMembers.entrySet()) {
            if (entry.getValue().size() < MAX_PER_ARENA) {
                entry.getValue().add(username);
                return entry.getKey();
            }
        }
        String arenaId = "arena-" + UUID.randomUUID().toString().substring(0, 8);
        Set<String> members = ConcurrentHashMap.newKeySet();
        members.add(username);
        arenaMembers.put(arenaId, members);
        matches.save(new Match(arenaId, username));
        return arenaId;
    }

    public void leaveArena(String arenaId, String username) {
        Set<String> members = arenaMembers.get(arenaId);
        if (members == null) return;
        members.remove(username);
        // Drop empty arenas so joinArena doesn't hand out a dead room.
        if (members.isEmpty()) arenaMembers.remove(arenaId);
    }

    public int playersIn(String arenaId) {
        Set<String> members = arenaMembers.get(arenaId);
        return members == null ? 0 : members.size();
    }

    public String websocketUrl() { return publicWsUrl; }
}
