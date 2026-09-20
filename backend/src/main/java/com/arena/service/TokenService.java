package com.arena.service;

import org.springframework.stereotype.Service;
import java.security.SecureRandom;
import java.util.Base64;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Opaque bearer tokens held in memory.
 *
 * Deliberately not JWT: this is a single backend process, tokens never need to
 * be validated by a third party, and an in-memory map means logout is actually
 * revocation rather than a blocklist. The tradeoff is that a restart logs
 * everyone out, and a second instance behind a load balancer would not share
 * the map -- swap in Redis or JWT at that point.
 */
@Service
public class TokenService {
    private final Map<String, String> tokenToUsername = new ConcurrentHashMap<>();
    private final SecureRandom random = new SecureRandom();

    public String issue(String username) {
        byte[] bytes = new byte[32];
        random.nextBytes(bytes);
        String token = Base64.getUrlEncoder().withoutPadding().encodeToString(bytes);
        tokenToUsername.put(token, username);
        return token;
    }

    public Optional<String> resolve(String token) {
        if (token == null || token.isBlank()) return Optional.empty();
        String bearer = token.startsWith("Bearer ") ? token.substring(7) : token;
        return Optional.ofNullable(tokenToUsername.get(bearer));
    }

    public void revoke(String token) {
        if (token != null) tokenToUsername.remove(token.replace("Bearer ", ""));
    }
}
