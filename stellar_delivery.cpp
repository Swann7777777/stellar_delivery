#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <enet/enet.h>
#include <SDL3/SDL.h>
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <cmath>
#include <unordered_map>














// Classes

struct player_struct {

    float x = 0;
    float y = 0;
    float xv = 0;
    float yv = 0;
    float speed = 100;
    int size = 1000;
};



struct planet_struct {

    int x;
    int y;
    int size;
    int index;
};



struct remote_player_struct {

    float x;
    float y;
    SDL_Color color;
};











// Initialization and cleanup functions

bool initialize() {
    if (enet_initialize() < 0) {
        std::cerr << "Enet could not initialize !" << std::endl;
        return false;
    }
    atexit(enet_deinitialize);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL could not initialize !" << std::endl;
        return -1;
    }

    if (!TTF_Init()) {
        std::cerr << "TTF could not initialize !" << std::endl;
        SDL_Quit();
        return false;
    }

    return true;
}




bool cleanup(SDL_Window* window, SDL_Renderer* renderer, TTF_Font* font, SDL_Surface* textSurface, SDL_Texture* textTexture) {
    if (font) {
        TTF_CloseFont(font);
    }

    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }

    if (window) {
        SDL_DestroyWindow(window);
    }

    if (textSurface) {
        SDL_DestroySurface(textSurface);
    }

    if (textTexture) {
        SDL_DestroyTexture(textTexture);
    }

    TTF_Quit();
    SDL_Quit();

    return true;
}












// Main function

int main() {


	// Initialize libraries

	initialize();




    // Create the player

    player_struct player;
    std::vector<planet_struct> planets;

    // Map to store other player positions
    // Key is the peer's address (host + port as unique identifier)
    std::unordered_map<uint64_t, remote_player_struct> other_players;

    ENetHost* client;
    client = enet_host_create(NULL, 1, 1, 0, 0);

    if (!client) {
        std::cerr << "Enet could not create a client host." << std::endl;
        return -1;
    }

    ENetAddress address;
    ENetEvent enet_event;
    ENetPeer* peer;

    enet_address_set_host(&address, "127.0.0.1");
    address.port = 16383;

    peer = enet_host_connect(client, &address, 1, 0);

    if (!peer) {
        std::cerr << "Enet could not establish connection." << std::endl;
        return -1;
    }

    if (enet_host_service(client, &enet_event, 5000) > 0 && enet_event.type == ENET_EVENT_TYPE_CONNECT) {
        std::cout << "Connection succeeded !" << std::endl;
    }
    else {
        enet_peer_reset(peer);
        std::cout << "Connection failed !" << std::endl;
        return -1;
    }

    // In your client code, modify the wait loop to include a timeout:
    bool wait = true;
    int timeout_attempts = 0;
    const int MAX_TIMEOUT = 10; // 10 second timeout

    // Wait for initial planet data
    while (wait && timeout_attempts < MAX_TIMEOUT) {
        if (enet_host_service(client, &enet_event, 1000) > 0) {
            switch (enet_event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                // Add a special message type or header to identify planet data
                if (enet_event.packet->dataLength > sizeof(float) * 2) {
                    std::cout << "Successfully received planet data!" << std::endl;

                    size_t planets_count = enet_event.packet->dataLength / sizeof(planet_struct);
                    std::cout << "Received " << planets_count << " planets" << std::endl;

                    planets.resize(planets_count);
                    std::memcpy(planets.data(), enet_event.packet->data, enet_event.packet->dataLength);

                    wait = false;
                }
                enet_packet_destroy(enet_event.packet);
                break;
            }
        }
        else {
            timeout_attempts++;
            std::cout << "Waiting for planet data... " << timeout_attempts << "/" << MAX_TIMEOUT << std::endl;
        }
    }

    if (timeout_attempts >= MAX_TIMEOUT) {
        std::cerr << "Timed out waiting for planet data" << std::endl;
        return -1;
    }

    SDL_DisplayID* displays;
    int display_count;
    displays = SDL_GetDisplays(&display_count);
    if (!displays) {
        std::cerr << "Failed to get displays: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    const SDL_DisplayMode* dm;
    dm = SDL_GetDesktopDisplayMode(displays[0]);
    if (!dm) {
        std::cerr << "Failed to get display mode: " << SDL_GetError() << std::endl;
        SDL_free(displays);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    int w = dm->w;
    int h = dm->h;
    SDL_free(displays);

    SDL_Window* window = SDL_CreateWindow("Space game", w, h, SDL_WINDOW_FULLSCREEN);
    if (!window) {
        std::cerr << "Window could not be created! SDL Error: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Load font
    TTF_Font* font = TTF_OpenFont("VCR_OSD_MONO.ttf", 24.0f);
    if (!font) {
        std::cerr << "Failed to load font: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    std::vector<SDL_Surface*> planet_images;

    // Load planet images
    for (int i = 1; i <= 5; ++i) {
        std::string path = "assets/planets/" + std::to_string(i) + ".png";
        SDL_Surface* image = IMG_Load(path.c_str());
        if (!image) {
            std::cerr << "Failed to load " << path << std::endl;
            return -1;
        }
        planet_images.push_back(image);
    }

    std::vector<SDL_Texture*> planet_textures;

    for (int i = 0; i < planets.size(); i++) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, planet_images[planets[i].index]);
        if (!texture) {
            std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
            return -1;
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        planet_textures.push_back(texture);
    }

    std::string fps_string = "FPS: --";
    std::string players_string = "Players: 1";
    SDL_Color textColor = { 200, 200, 200, 255 };
    SDL_Surface* textSurface = nullptr;
    SDL_Texture* textTexture = nullptr;
    SDL_FRect textRect = { 10, 10, 0, 0 };

    SDL_Surface* playerCountSurface = nullptr;
    SDL_Texture* playerCountTexture = nullptr;
    SDL_FRect playerCountRect = { 10, 40, 0, 0 };




    SDL_Event event;
    float zoom = 1;
    bool run = true;
    Uint64 old_ticks = SDL_GetTicks();

    float time = 0.0f;
    float network_time = 0.0f; // Separate timer for network updates
    int frameCount = 0;
    int currentFPS = 0;
    float average_dt = 0.0f;

    while (run) {
        Uint64 ticks = SDL_GetTicks();
        float dt = (ticks - old_ticks) / 1000.0f;
        old_ticks = ticks;

        



        // FPS calculation
        time += dt;
        network_time += dt;
        frameCount++;

        // Update FPS and player count text every second
        if (time >= 1.0f) {

            average_dt = time / frameCount;




            currentFPS = frameCount;
            fps_string = "FPS: " + std::to_string(currentFPS);
            players_string = "Players: " + std::to_string(other_players.size() + 1); // +1 for local player
            frameCount = 0;

            // Update FPS text texture
            if (textSurface) {
                SDL_DestroySurface(textSurface);
                textSurface = nullptr;
            }
            if (textTexture) {
                SDL_DestroyTexture(textTexture);
                textTexture = nullptr;
            }

            textSurface = TTF_RenderText_Blended(font, fps_string.c_str(), fps_string.length(), textColor);
            if (textSurface) {
                textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                textRect.w = textSurface->w;
                textRect.h = textSurface->h;
            }

            // Update player count text texture
            if (playerCountSurface) {
                SDL_DestroySurface(playerCountSurface);
                playerCountSurface = nullptr;
            }
            if (playerCountTexture) {
                SDL_DestroyTexture(playerCountTexture);
                playerCountTexture = nullptr;
            }

            playerCountSurface = TTF_RenderText_Blended(font, players_string.c_str(), players_string.length(), textColor);
            if (playerCountSurface) {
                playerCountTexture = SDL_CreateTextureFromSurface(renderer, playerCountSurface);
                playerCountRect.w = playerCountSurface->w;
                playerCountRect.h = playerCountSurface->h;
            }

            time = 0.0f;
        }

        // Event handling
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                run = false;
            }
        }

        // Network event handling - receive positions of other players
        while (enet_host_service(client, &enet_event, 1) > 0) {
            switch (enet_event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                // Check if this is a position update packet (should be exactly 2 floats)
                if (enet_event.packet->dataLength == sizeof(float) * 2) {
                    float x, y;
                    std::memcpy(&x, enet_event.packet->data, sizeof(float));
                    std::memcpy(&y, enet_event.packet->data + sizeof(float), sizeof(float));

                    // Create a unique identifier for this player using address info
                    uint64_t player_id = (uint64_t)enet_event.peer->address.host << 32 | enet_event.peer->address.port;

                    // Add or update the player position
                    if (other_players.find(player_id) == other_players.end()) {
                        // New player, generate a random color
                        other_players[player_id] = { x, y, {255, 0, 0, 255} };
                    }
                    else {
                        // Update existing player position
                        other_players[player_id].x = x;
                        other_players[player_id].y = y;
                    }
                }
                enet_packet_destroy(enet_event.packet);
                break;
            }
        }

        // Keyboard input
        const bool* state = SDL_GetKeyboardState(NULL);

        if (state[SDL_SCANCODE_ESCAPE]) {
            run = false;
        }
        if (state[SDL_SCANCODE_UP]) {
            player.yv += player.speed;
        }
        if (state[SDL_SCANCODE_DOWN]) {
            player.yv -= player.speed;
        }
        if (state[SDL_SCANCODE_RIGHT]) {
            player.xv += player.speed;
        }
        if (state[SDL_SCANCODE_LEFT]) {
            player.xv -= player.speed;
        }
        if (state[SDL_SCANCODE_W]) {
            zoom *= 1.0f + (0.5f * dt);
        }
        if (state[SDL_SCANCODE_S]) {
            zoom *= 1.0f - (0.3f * dt);
        }

        // Gravitational effects
        for (int i = 0; i < planets.size(); i++) {
            float dx = planets[i].x - player.x;
            float dy = planets[i].y - player.y;


			if (abs(dx) <= planets[i].size && abs(dy) <= planets[i].size) {
                if (sqrt(pow(planets[i].x - player.x, 2) + pow(planets[i].y - player.y, 2)) < planets[i].size / 2.0f) {
                    //player.x = 0;
                    //player.y = 0;
                    //player.xv = 0;
                    //player.yv = 0;
                    //zoom = 1;
                }
			}



            float distance = sqrt((dx * dx) + (dy * dy));

			float distance_treshold = planets[i].size * 100.0f;



            if (distance < distance_treshold) {
                // Scale the gravitational force to make it more noticeable
                float gravitationalConstant = 300000.0f; // Adjust this value as needed
                float force = (gravitationalConstant * planets[i].size) / distance;

                // Normalize the direction and apply the force
                player.xv += force * (dx / distance) * dt;
                player.yv += force * (dy / distance) * dt;
            }




        }


        // Update player position and velocity
        player.x += player.xv * dt;
        player.y += player.yv * dt;
        //player.xv *= pow(0.9f, dt);
        //player.yv *= pow(0.9f, dt);

        // Send position updates every 0.1 seconds (adjust as needed for your game)
        if (network_time >= 0.1f) {
            char buffer[sizeof(float) * 2];
            memcpy(buffer, &player.x, sizeof(float));
            memcpy(buffer + sizeof(float), &player.y, sizeof(float));

            ENetPacket* packet = enet_packet_create(buffer, sizeof(buffer), ENET_PACKET_FLAG_RELIABLE);
            if (packet) {
                if (enet_peer_send(peer, 0, packet) != 0) {
                    std::cerr << "Failed to send position packet!" << std::endl;
                    enet_packet_destroy(packet);
                }
            }

            network_time = 0.0f;
        }

        // Render background
        SDL_SetRenderDrawColor(renderer, 0, 0, 50, 255);
        SDL_RenderClear(renderer);

        // Render planets
        for (int i = 0; i < planets.size(); i++) {
            float dx = abs(planets[i].x - player.x);
            float dy = abs(planets[i].y - player.y);


			if (dx < (w / 2) * zoom || dy < (h / 2) * zoom) {
                SDL_FRect planet_rect = {
                    (w / 2) + ((planets[i].x - player.x) / zoom) - ((planets[i].size / zoom) / 2),
                    (h / 2) - ((planets[i].y - player.y) / zoom) - ((planets[i].size / zoom) / 2),
                    planets[i].size / zoom,
                    planets[i].size / zoom };

                SDL_RenderTexture(renderer, planet_textures[i], NULL, &planet_rect);
			}

        }

        // Render other players
        // Render other players
        for (auto it = other_players.begin(); it != other_players.end(); ++it) {
            const remote_player_struct& remote = it->second;

            SDL_FRect remote_player_rect = {
                (w / 2) + ((remote.x - player.x) / zoom) - ((player.size / zoom) / 2),
                (h / 2) - ((remote.y - player.y) / zoom) - ((player.size / zoom) / 2),
                player.size / zoom, player.size / zoom };

            // Fix SDL_SetRenderDrawColor (needs 5 args: renderer, r, g, b, a)
            SDL_SetRenderDrawColor(renderer,
                remote.color.r,
                remote.color.g,
                remote.color.b,
                remote.color.a);
            SDL_RenderFillRect(renderer, &remote_player_rect);
        }

        // Render player
        SDL_FRect player_rect = {
            (w / 2) - ((player.size / zoom) / 2),
            (h / 2) - ((player.size / zoom) / 2),
            player.size / zoom, player.size / zoom };

        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderFillRect(renderer, &player_rect);

        // Render FPS text
        if (textTexture) {
            SDL_RenderTexture(renderer, textTexture, NULL, &textRect);
        }

        // Render player count text
        if (playerCountTexture) {
            SDL_RenderTexture(renderer, playerCountTexture, NULL, &playerCountRect);
        }







        SDL_RenderLine(renderer, w / 2, h / 2, w / 2 - (player.xv / zoom) * average_dt * 30, h / 2 + (player.yv / zoom) * average_dt * 30);


        // Present renderer
        SDL_RenderPresent(renderer);

        // Clean up players that haven't sent updates in a while (implement if needed)
        // You could add a timestamp to each player and remove those who haven't updated in X seconds
    }

    // Clean up planet textures
    for (auto texture : planet_textures) {
        SDL_DestroyTexture(texture);
    }

    // Clean up planet images
    for (auto surface : planet_images) {
        SDL_DestroySurface(surface);
    }

    // Disconnect gracefully
    enet_peer_disconnect(peer, 0);

    while (enet_host_service(client, &enet_event, 3000) > 0) {
        switch (enet_event.type) {
        case ENET_EVENT_TYPE_RECEIVE:
            enet_packet_destroy(enet_event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            std::cout << "Disconnection succeeded !" << std::endl;
            break;
        }
    }

    enet_host_destroy(client);

    if (!cleanup(window, renderer, font, textSurface, textTexture)) {
        std::cerr << "Cleanup failed!" << std::endl;
        return -1;
    }

    return 0;
}