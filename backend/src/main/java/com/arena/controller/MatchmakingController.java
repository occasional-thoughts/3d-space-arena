package com.arena.controller;

import com.arena.dto.Dtos.*;
import com.arena.service.MatchmakingService;
import com.arena.service.TokenService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/matchmaking")
@CrossOrigin(origins = "*")
public class MatchmakingController {

    private final MatchmakingService matchmaking;
    private final TokenService tokens;

    public MatchmakingController(MatchmakingService matchmaking, TokenService tokens) {
        this.matchmaking = matchmaking;
        this.tokens = tokens;
    }

    @PostMapping("/join")
    public ResponseEntity<?> join(@RequestHeader(value = "Authorization", required = false) String auth) {
        var username = tokens.resolve(auth);
        if (username.isEmpty())
            return ResponseEntity.status(401).body(new ErrorResponse("missing or invalid token"));

        String arenaId = matchmaking.joinArena(username.get());
        return ResponseEntity.ok(new QueueResponse(
                arenaId,
                matchmaking.websocketUrl() + "?arena=" + arenaId,
                matchmaking.playersIn(arenaId)));
    }
}
