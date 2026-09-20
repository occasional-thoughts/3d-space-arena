package com.arena.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;

public class Dtos {

    public record AuthRequest(
            @NotBlank @Size(min = 3, max = 32) String username,
            @NotBlank @Size(min = 8, max = 128) String password) {}

    public record AuthResponse(String token, String username, int score, int wins) {}

    public record LeaderboardRow(String username, int score, int wins, int kills) {}

    public record QueueResponse(String arenaId, String websocketUrl, int playersWaiting) {}

    public record ErrorResponse(String error) {}
}
