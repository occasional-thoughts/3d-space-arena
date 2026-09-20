package com.arena;

import com.arena.dto.Dtos.*;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.*;

import static org.junit.jupiter.api.Assertions.*;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class ApiSmokeTest {

    @Autowired
    private TestRestTemplate rest;

    private HttpEntity<AuthRequest> body(String user, String pass) {
        HttpHeaders headers = new HttpHeaders();
        headers.setContentType(MediaType.APPLICATION_JSON);
        return new HttpEntity<>(new AuthRequest(user, pass), headers);
    }

    @Test
    void registersThenRejectsDuplicate() {
        var first = rest.postForEntity("/auth/register", body("pilot_a", "longenough1"), AuthResponse.class);
        assertEquals(HttpStatus.OK, first.getStatusCode());
        assertNotNull(first.getBody().token());

        var dup = rest.postForEntity("/auth/register", body("pilot_a", "longenough1"), String.class);
        assertEquals(HttpStatus.CONFLICT, dup.getStatusCode());
    }

    @Test
    void rejectsWrongPassword() {
        rest.postForEntity("/auth/register", body("pilot_b", "longenough1"), String.class);
        var bad = rest.postForEntity("/auth/login", body("pilot_b", "wrongpassword"), String.class);
        assertEquals(HttpStatus.UNAUTHORIZED, bad.getStatusCode());
    }

    @Test
    void matchmakingRequiresToken() {
        var anon = rest.postForEntity("/matchmaking/join", HttpEntity.EMPTY, String.class);
        assertEquals(HttpStatus.UNAUTHORIZED, anon.getStatusCode());
    }

    @Test
    void twoPlayersShareAnArena() {
        var a = rest.postForEntity("/auth/register", body("pilot_c", "longenough1"), AuthResponse.class);
        var b = rest.postForEntity("/auth/register", body("pilot_d", "longenough1"), AuthResponse.class);

        String arenaA = join(a.getBody().token()).arenaId();
        var second = join(b.getBody().token());

        // The whole point of dropping the four-player queue: player two lands
        // in the same arena and can fly immediately.
        assertEquals(arenaA, second.arenaId());
        assertTrue(second.playersWaiting() >= 2);
    }

    private QueueResponse join(String token) {
        HttpHeaders h = new HttpHeaders();
        h.setBearerAuth(token);
        var res = rest.exchange("/matchmaking/join", HttpMethod.POST,
                new HttpEntity<>(h), QueueResponse.class);
        assertEquals(HttpStatus.OK, res.getStatusCode());
        return res.getBody();
    }

    @Test
    void leaderboardIsPublic() {
        var res = rest.getForEntity("/leaderboard", String.class);
        assertEquals(HttpStatus.OK, res.getStatusCode());
    }
}
