package com.arena.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "matches")
public class Match {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, length = 64)
    private String arenaId;

    // Comma-joined usernames. A join table would be the textbook answer, but
    // nothing queries by participant yet, so this stays one row per match.
    @Column(nullable = false, length = 512)
    private String players;

    @Column(length = 32)
    private String winner;

    @Column(nullable = false)
    private LocalDateTime startedAt = LocalDateTime.now();

    private LocalDateTime endedAt;

    public Match() {}

    public Match(String arenaId, String players) {
        this.arenaId = arenaId;
        this.players = players;
    }

    public Long getId() { return id; }
    public String getArenaId() { return arenaId; }
    public String getPlayers() { return players; }
    public String getWinner() { return winner; }
    public void setWinner(String v) { this.winner = v; }
    public LocalDateTime getStartedAt() { return startedAt; }
    public LocalDateTime getEndedAt() { return endedAt; }
    public void setEndedAt(LocalDateTime v) { this.endedAt = v; }
}
