package com.arena.controller;

import com.arena.dto.Dtos.LeaderboardRow;
import com.arena.repository.UserRepository;
import org.springframework.data.domain.PageRequest;
import org.springframework.web.bind.annotation.*;
import java.util.List;

@RestController
@CrossOrigin(origins = "*")
public class LeaderboardController {

    private final UserRepository users;

    public LeaderboardController(UserRepository users) {
        this.users = users;
    }

    @GetMapping("/leaderboard")
    public List<LeaderboardRow> leaderboard(@RequestParam(defaultValue = "20") int limit) {
        int capped = Math.min(Math.max(limit, 1), 100);
        return users.findAllByOrderByScoreDescWinsDesc(PageRequest.of(0, capped))
                .stream()
                .map(u -> new LeaderboardRow(u.getUsername(), u.getScore(), u.getWins(), u.getKills()))
                .toList();
    }
}
