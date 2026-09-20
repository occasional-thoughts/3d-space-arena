package com.arena.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "users", indexes = @Index(name = "idx_users_score", columnList = "score DESC"))
public class User {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true, length = 32)
    private String username;

    @Column(nullable = false)
    private String passwordHash;

    @Column(nullable = false)
    private int score = 0;

    @Column(nullable = false)
    private int wins = 0;

    @Column(nullable = false)
    private int kills = 0;

    @Column(nullable = false)
    private LocalDateTime createdAt = LocalDateTime.now();

    public Long getId() { return id; }
    public String getUsername() { return username; }
    public void setUsername(String v) { this.username = v; }
    public String getPasswordHash() { return passwordHash; }
    public void setPasswordHash(String v) { this.passwordHash = v; }
    public int getScore() { return score; }
    public void setScore(int v) { this.score = v; }
    public int getWins() { return wins; }
    public void setWins(int v) { this.wins = v; }
    public int getKills() { return kills; }
    public void setKills(int v) { this.kills = v; }
    public LocalDateTime getCreatedAt() { return createdAt; }
}
