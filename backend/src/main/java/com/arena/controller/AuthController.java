package com.arena.controller;

import com.arena.dto.Dtos.*;
import com.arena.entity.User;
import com.arena.repository.UserRepository;
import com.arena.service.TokenService;
import jakarta.validation.Valid;
import org.springframework.http.ResponseEntity;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/auth")
@CrossOrigin(origins = "*")
public class AuthController {

    private final UserRepository users;
    private final TokenService tokens;
    private final BCryptPasswordEncoder encoder = new BCryptPasswordEncoder();

    public AuthController(UserRepository users, TokenService tokens) {
        this.users = users;
        this.tokens = tokens;
    }

    @PostMapping("/register")
    public ResponseEntity<?> register(@Valid @RequestBody AuthRequest req) {
        if (users.existsByUsername(req.username()))
            return ResponseEntity.status(409).body(new ErrorResponse("username taken"));

        User u = new User();
        u.setUsername(req.username());
        u.setPasswordHash(encoder.encode(req.password()));
        users.save(u);

        String token = tokens.issue(u.getUsername());
        return ResponseEntity.ok(new AuthResponse(token, u.getUsername(), 0, 0));
    }

    @PostMapping("/login")
    public ResponseEntity<?> login(@Valid @RequestBody AuthRequest req) {
        var found = users.findByUsername(req.username());
        // Same response for unknown user and bad password: telling them apart
        // turns this endpoint into a username enumerator.
        if (found.isEmpty() || !encoder.matches(req.password(), found.get().getPasswordHash()))
            return ResponseEntity.status(401).body(new ErrorResponse("invalid credentials"));

        User u = found.get();
        String token = tokens.issue(u.getUsername());
        return ResponseEntity.ok(new AuthResponse(token, u.getUsername(), u.getScore(), u.getWins()));
    }

    @PostMapping("/logout")
    public ResponseEntity<?> logout(@RequestHeader(value = "Authorization", required = false) String auth) {
        tokens.revoke(auth);
        return ResponseEntity.noContent().build();
    }
}
