// ── JFinal4.cpp ─────────────────────────────────────────────────────────────
// Final version of the Castle Adventure game with joystick input on Raspberry Pi.
// This file is a complete, self-contained C++ program that implements the game logic,
// world setup, and serial communication with the Arduino R3 for joystick input.
// Author: Viveka Patterson
// Used Claude for code review and bug fixes, but all code in this file was written by me.

#include <iostream>
#include <cstring>
#include <chrono>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <wiringPi.h>

using namespace std;

int serialFile = -1;
const char* SERIAL_PORT_PATH = "/dev/ttyACM0"; // Change to /dev/ttyUSB0 if connection drops


const int MAX_LOCATIONS = 22;
const int MAX_EXITS     = 4;
const int MAX_ITEMS     = 3;

// Data Structures
struct ItemNode {
    const char* itemName;
    ItemNode*   next;
};

struct Exit {
    const char* direction;
    int         locationIndex;
};

struct RoomItem {
    const char* name;
    bool        collected;
};

struct Location {
    const char* name;
    const char* description;
    Exit        exits[MAX_EXITS];
    int         exitCount;
    RoomItem    items[MAX_ITEMS];
    int         itemCount;
};

// ── USB Serial Setup ──────────────────────────────────────────────────────────
void setupJoystick() {
    serialFile = open(SERIAL_PORT_PATH, O_RDONLY | O_NOCTTY | O_NONBLOCK);

    if (serialFile < 0) {
        cout << "[System Error] Cannot locate Arduino R3 on " << SERIAL_PORT_PATH << endl;
        cout << "Please verify USB cable connection pins." << endl;
        return;
    }

    struct termios tty;
    if (tcgetattr(serialFile, &tty) != 0) return;

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~ICANON;
    tty.c_cc[VTIME] = 1;
    tty.c_cc[VMIN]  = 0;

    tcsetattr(serialFile, TCSANOW, &tty);
    tcflush(serialFile, TCIFLUSH);
}

// ── Read Actions from Serial Stream ───────────────────────────────────────────

const char* waitForJoystickInput(int timeoutMs = 10000) {
    static char command[20];
    if (serialFile < 0) {
        cout << "[Error] Serial port unavailable. Check USB connection." << endl;
        delay(2000);
        return nullptr;
    }
    
    char readBuffer;
    string commandAccumulator = "";

    auto start = chrono::high_resolution_clock::now();
    
    while (true) {
        auto now = chrono::high_resolution_clock::now();
        int elsapsedMs = chrono::duration_cast<chrono::milliseconds>(now - start).count();

        if (elsapsedMs >= timeoutMs) {
            cout << "[Error] No input received within timeout period. Check joystick connection." << endl;
            return nullptr;
        }    
        
        int bytesRead = read(serialFile, &readBuffer, 1);

        if (bytesRead > 0) {
            if (readBuffer == '\r') {
                
                continue;
            } else if (readBuffer == '\n') {
                if (!commandAccumulator.empty()) {
                    strcpy(command, commandAccumulator.c_str());
                    if (commandAccumulator == "inspect") return "inspect";
                    if (commandAccumulator == "west")    return "west";
                    if (commandAccumulator == "east")    return "east";
                    if (commandAccumulator == "north")   return "north";
                    if (commandAccumulator == "south")   return "south";
                    commandAccumulator = "";
                }
            } else {
                commandAccumulator += readBuffer;
            }
        }
        delay(10);
    }
}

// ── Game ──────────────────────────────────────────────────────────────────────

class Game {
private:
    Location  locations[MAX_LOCATIONS];
    int       currentLocation;
    ItemNode* inventoryHead;

public:
    Game() : currentLocation(0), inventoryHead(NULL) { setupWorld(); }
    
    ~Game() {
        ItemNode* current = inventoryHead;
        while (current != NULL) {
            ItemNode* next = current->next;
            delete current;
            current = next;
        }
    }

    //Array of locations with their descriptions, exits, and items

    void setupWorld() {
        locations[0].name        = "Castle Courtyard";
        locations[0].description = "You are standing in the castle courtyard.";
        locations[0].exitCount   = 4;
        locations[0].exits[0]    = {"north", 1};
        locations[0].exits[1]    = {"east",  2};
        locations[0].exits[2]    = {"west",  3};
        locations[0].exits[3]    = {"south", 4};
        locations[0].itemCount   = 0;

        locations[1].name        = "Forest";
        locations[1].description = "Tall trees surround the castle.";
        locations[1].exitCount   = 4;
        locations[1].exits[0]    = {"south", 0};
        locations[1].exits[1]    = {"west",  19};
        locations[1].exits[2]    = {"east",  20};
        locations[1].exits[3]    = {"north", 21};
        locations[1].itemCount   = 2;
        locations[1].items[0]    = {"Magic Leaf",   false};
        locations[1].items[1]    = {"Wooden Stick", false};

        locations[2].name        = "River";
        locations[2].description = "A cold river flows beside the castle.";
        locations[2].exitCount   = 2;
        locations[2].exits[0]    = {"west",  0};
        locations[2].exits[1]    = {"north", 18};
        locations[2].itemCount   = 2;
        locations[2].items[0]    = {"Smooth Stone", false};
        locations[2].items[1]    = {"River Shell",  false};

        locations[3].name        = "Village";
        locations[3].description = "A small village sits outside the castle walls.";
        locations[3].exitCount   = 2;
        locations[3].exits[0]    = {"east", 0};
        locations[3].exits[1]    = {"west", 14};
        locations[3].itemCount   = 1;
        locations[3].items[0]    = {"Old Map", false};

        locations[4].name        = "Front Door";
        locations[4].description = "The large castle doors stand in front of you.";
        locations[4].exitCount   = 2;
        locations[4].exits[0]    = {"north", 0};
        locations[4].exits[1]    = {"south", 5};
        locations[4].itemCount   = 0;

        locations[5].name        = "Castle Foyer";
        locations[5].description = "You enter the main foyer of the castle.";
        locations[5].exitCount   = 4;
        locations[5].exits[0]    = {"north", 4};
        locations[5].exits[1]    = {"south", 6};
        locations[5].exits[2]    = {"east",  10};
        locations[5].exits[3]    = {"west",  13};
        locations[5].itemCount   = 1;
        locations[5].items[0]    = {"Candle", false};

        locations[6].name        = "Castle Interior";
        locations[6].description = "You are deep inside the castle.";
        locations[6].exitCount   = 4;
        locations[6].exits[0]    = {"north", 5};
        locations[6].exits[1]    = {"south", 7};
        locations[6].exits[2]    = {"east",  16};
        locations[6].exits[3]    = {"west",  15};
        locations[6].itemCount   = 0;

        locations[7].name        = "Throne Room";
        locations[7].description = "A royal throne sits at the center of the room.";
        locations[7].exitCount   = 4;
        locations[7].exits[0]    = {"north", 6};
        locations[7].exits[1]    = {"south", 8};
        locations[7].exits[2]    = {"west",  15};
        locations[7].exits[3]    = {"east",  16};
        locations[7].itemCount   = 2;
        locations[7].items[0]    = {"Royal Coin",   false};
        locations[7].items[1]    = {"Golden Crown", false};

        locations[8].name        = "Treasure Room";
        locations[8].description = "Gold and jewels sparkle around you.";
        locations[8].exitCount   = 2;
        locations[8].exits[0]    = {"north", 7};
        locations[8].exits[1]    = {"south", 9};
        locations[8].itemCount   = 3;
        locations[8].items[0]    = {"Crown Jewel", false};
        locations[8].items[1]    = {"Gold Bar",    false};
        locations[8].items[2]    = {"Ruby",        false};

        locations[9].name        = "Secret Passage";
        locations[9].description = "A hidden passage leads away from the treasure room.";
        locations[9].exitCount   = 4;
        locations[9].exits[0]    = {"north", 8};
        locations[9].exits[1]    = {"south", 1};
        locations[9].exits[2]    = {"west",  18};
        locations[9].exits[3]    = {"east",  3};
        locations[9].itemCount   = 1;
        locations[9].items[0]    = {"Secret Note", false};

        locations[10].name        = "Armory";
        locations[10].description = "Weapons and shields line the walls.";
        locations[10].exitCount   = 2;
        locations[10].exits[0]    = {"west", 5};
        locations[10].exits[1]    = {"east", 11};
        locations[10].itemCount   = 2;
        locations[10].items[0]    = {"Silver Sword", false};
        locations[10].items[1]    = {"Shield",       false};

        locations[11].name        = "Soldier's Quarters";
        locations[11].description = "Small beds and armor racks fill the room.";
        locations[11].exitCount   = 2;
        locations[11].exits[0]    = {"west", 10};
        locations[11].exits[1]    = {"east", 12};
        locations[11].itemCount   = 1;
        locations[11].items[0]    = {"Helmet", false};

        locations[12].name        = "Dungeon";
        locations[12].description = "The dungeon is dark and cold.";
        locations[12].exitCount   = 2;
        locations[12].exits[0]    = {"west",  11};
        locations[12].exits[1]    = {"south", 19};
        locations[12].itemCount   = 2;
        locations[12].items[0]    = {"Rusty Chain", false};
        locations[12].items[1]    = {"Iron Key",    false};

        locations[13].name        = "King's Chambers";
        locations[13].description = "This room belongs to the king.";
        locations[13].exitCount   = 3;
        locations[13].exits[0]    = {"east",  5};
        locations[13].exits[1]    = {"south", 15};
        locations[13].exits[2]    = {"west",  14};
        locations[13].itemCount   = 2;
        locations[13].items[0]    = {"Royal Ring",    false};
        locations[13].items[1]    = {"King's Letter", false};

        locations[14].name        = "Garden";
        locations[14].description = "Flowers grow in the castle garden.";
        locations[14].exitCount   = 3;
        locations[14].exits[0]    = {"east",  13};
        locations[14].exits[1]    = {"south", 16};
        locations[14].exits[2]    = {"west",  7};  
        locations[14].itemCount   = 2;
        locations[14].items[0]    = {"Red Flower", false};
        locations[14].items[1]    = {"Garden Key", false};

        locations[15].name        = "Library";
        locations[15].description = "Dusty books fill tall shelves.";
        locations[15].exitCount   = 3;
        locations[15].exits[0]    = {"north", 13};
        locations[15].exits[1]    = {"south", 14};
        locations[15].exits[2]    = {"east",  6};
        locations[15].itemCount   = 3;
        locations[15].items[0]    = {"Spell Book", false};
        locations[15].items[1]    = {"Old Scroll", false};
        locations[15].items[2]    = {"Magic Ink",  false};

        locations[16].name        = "Ballroom";
        locations[16].description = "The ballroom has a large shiny floor.";
        locations[16].exitCount   = 4;
        locations[16].exits[0]    = {"west",  6};
        locations[16].exits[1]    = {"east",  17};
        locations[16].exits[2]    = {"north", 14};
        locations[16].exits[3]    = {"south", 7};
        locations[16].itemCount   = 1;
        locations[16].items[0]    = {"Glass Slipper", false};

        locations[17].name        = "Kitchen";
        locations[17].description = "The kitchen smells like old bread.";
        locations[17].exitCount   = 1;
        locations[17].exits[0]    = {"west", 16};
        locations[17].itemCount   = 2;
        locations[17].items[0]    = {"Bread",         false};
        locations[17].items[1]    = {"Kitchen Knife", false};

        locations[18].name        = "Cave";
        locations[18].description = "A dark cave waits near the river.";
        locations[18].exitCount   = 2;
        locations[18].exits[0]    = {"south", 2};
        locations[18].exits[1]    = {"east",  9};
        locations[18].itemCount   = 2;
        locations[18].items[0]    = {"Ancient Key",  false};
        locations[18].items[1]    = {"Crystal Rock", false};

        locations[19].name        = "Escape Tunnel";
        locations[19].description = "A narrow tunnel leads out of the dungeon.";
        locations[19].exitCount   = 2;
        locations[19].exits[0]    = {"north", 12};
        locations[19].exits[1]    = {"south", 1};
        locations[19].itemCount   = 1;
        locations[19].items[0]    = {"Torch", false};

        locations[20].name        = "Mountain";
        locations[20].description = "You climb a rocky mountain path.";
        locations[20].exitCount   = 1;
        locations[20].exits[0]    = {"west", 1};
        locations[20].itemCount   = 2;
        locations[20].items[0]    = {"Golden Feather",   false};
        locations[20].items[1]    = {"Mountain Crystal", false};

        locations[21].name        = "Hidden Tunnel";
        locations[21].description = "A hidden tunnel is covered by vines.";
        locations[21].exitCount   = 1;
        locations[21].exits[0]    = {"east", 1}; 
        locations[21].itemCount   = 1;
        locations[21].items[0]    = {"Hidden Gem", false};
    }
    // ── Game Logic ─────────────────────────────────────────────────────────────
    // Display current location, description, exits, and items

    void showLocation() {
        cout << "\nYou are at the " << locations[currentLocation].name << "." << endl;
        cout << locations[currentLocation].description << endl;

        cout << "Available paths:" << endl;
        for (int i = 0; i < locations[currentLocation].exitCount; i++) {
            int destIndex = locations[currentLocation].exits[i].locationIndex;
            cout << "  " << locations[currentLocation].exits[i].direction
                 << " -> " << locations[destIndex].name << endl;
        }

        if (locations[currentLocation].itemCount > 0) {
            cout << "Items here:" << endl;
            for (int i = 0; i < locations[currentLocation].itemCount; i++) {
                cout << "  - " << locations[currentLocation].items[i].name;
                if (locations[currentLocation].items[i].collected)
                    cout << " (collected)";
                cout << endl;
            }
        } else {
            cout << "No items here." << endl;
        }
    }
    // Move to a new location based on direction input
    // Check if the direction is valid from the current location and update currentLocation
    
    void move(const char* direction) {
        if (direction == nullptr) {
            cout << "No input received." << endl;
            return;
        }
        for (int i = 0; i < locations[currentLocation].exitCount; i++) {
            if (strcmp(locations[currentLocation].exits[i].direction, direction) == 0) {
                currentLocation = locations[currentLocation].exits[i].locationIndex;
                cout << "You moved " << direction << endl;
                return;
            }
        }
        cout << "You cannot go that way." << endl;
    }

    // Inspect the current location for items and allow the player to collect them
    // Display items in the room and allow selection with joystick input
    // If the player selects an item, add it to their inventory and mark it as collected
    // Allow exiting the inspection mode with the up button
    void inspect() {
        if (locations[currentLocation].itemCount == 0) {
            cout << "Nothing found here." << endl;
            return;
        }

        int selection = 0;
        
        while (true) {
            cout << "\nItems in Room:" << endl;
            for (int i = 0; i < locations[currentLocation].itemCount; i++) {
                cout << (i == selection ? "> " : "  ");
                cout << locations[currentLocation].items[i].name;
                if (locations[currentLocation].items[i].collected)
                    cout << " - collected";
                cout << endl;
            }
            cout << "Left/Right to select, button to pick up, up to exit." << endl;

            const char* input = waitForJoystickInput();
            
            if (input == nullptr) continue;

            if (strcmp(input, "west") == 0) {
                if (selection > 0) selection--;
            } else if (strcmp(input, "east") == 0) {
                if (selection < locations[currentLocation].itemCount - 1) selection++;
            } else if (strcmp(input, "inspect") == 0) {
                if (!locations[currentLocation].items[selection].collected) {
                    addItem(locations[currentLocation].items[selection].name);
                    locations[currentLocation].items[selection].collected = true;
                } else {
                    cout << "You already collected that item." << endl;
                }
            } else if (strcmp(input, "north") == 0) {
                break;
            }
        }
    }
    // Add an item to the player's inventory if they don't already have it
    // Check if the item is already in the inventory before adding
    // If the item is new, add it to the front of the linked list inventory
     
    void addItem(const char* itemName) {
        ItemNode* current = inventoryHead;
        // Traverse the linked list to check if the item is already in the inventory
        while (current != NULL) {
            if (strcmp(current->itemName, itemName) == 0) {
                cout << "You already have the " << itemName << endl;
                return;
            }
            current = current->next;
        }
        ItemNode* newItem = new ItemNode;
        newItem->itemName = itemName;
        newItem->next     = inventoryHead;
        inventoryHead     = newItem;
        cout << "You found: " << itemName << endl;
    }

    // Display the player's current inventory of collected items
    // If the inventory is empty, display "Inventory: Empty". Otherwise, list all collected items.
    // Traverse the linked list of inventory items and print their names
    // If the inventory is empty, print "Inventory: Empty"
    // If the inventory has items, print each item name followed by a comma
    // After listing all items, print a newline

    void showInventory() {
        cout << "Inventory: ";
        if (inventoryHead == NULL) {
            cout << "Empty" << endl;
        } else {
            ItemNode* current = inventoryHead;
            // Traverse the linked list and print item names
            while (current != NULL) {
                cout << current->itemName << ", ";
                current = current->next;
            }
            cout << endl;
        }
    }

    int getLocation() { return currentLocation; }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

void runTests(Game& testGame) {
    cout << "\n=== Running Tests ===" << endl;

    // Flush once here so stale bytes from Arduino startup don't corrupt Test 1
    if (serialFile >= 0) tcflush(serialFile, TCIFLUSH);

    cout << "Test 1: Push NORTH (Courtyard -> Forest)" << endl;
    const char* input = waitForJoystickInput();
    if (input != nullptr) {
        testGame.move(input);
        cout << (testGame.getLocation() == 1 ? "Test 1 PASSED" : "Test 1 FAILED") << endl;
    } else {
        cout << "Test 1 SKIPPED (no serial input)" << endl;
    }

    cout << "Test 2: Push SOUTH (Forest -> Courtyard)" << endl;
    input = waitForJoystickInput();
    if (input != nullptr) {
        testGame.move(input);
        cout << (testGame.getLocation() == 0 ? "Test 2 PASSED" : "Test 2 FAILED") << endl;
    } else {
        cout << "Test 2 SKIPPED (no serial input)" << endl;
    }

    cout << "Test 3: Push SOUTH (Courtyard -> Front Door)" << endl;
    input = waitForJoystickInput();
    if (input != nullptr) {
        testGame.move(input);
        cout << (testGame.getLocation() == 4 ? "Test 3 PASSED" : "Test 3 FAILED") << endl;
    } else {
        cout << "Test 3 SKIPPED (no serial input)" << endl;
    }

    cout << "=== Tests complete. Starting game... ===" << endl;
}

// ── Setup & Loop ──────────────────────────────────────────────────────────────

int main() {
    wiringPiSetup();
    setupJoystick();

    // ── Run Tests ────────────────────────────────────────────────────────────
    Game testGame;
    runTests(testGame);

    // ── Start Game ───────────────────────────────────────────────────────────
    Game* game = new Game();
    cout << "\nCastle Adventure Game Started!" << endl;
    cout << "Joystick to move, button to inspect." << endl;

    // Flush any bytes that accumulated during tests before entering the game loop
    if (serialFile >= 0) tcflush(serialFile, TCIFLUSH);

    // ── Main Game Loop ───────────────────────────────────────────────────────
    while (true) {
        game->showLocation();
        game->showInventory();

        // Performance tracking: measure time per game loop iteration
        auto loopStart = chrono::high_resolution_clock::now();

        const char* action = waitForJoystickInput();
        
        if (action == nullptr) continue;

        if (strcmp(action, "inspect") == 0) {
            game->inspect();
        } else {
            game->move(action);
        }

        // Execution time output (requirement e)
        auto loopEnd  = chrono::high_resolution_clock::now();
        auto elapsed  = chrono::duration_cast<chrono::milliseconds>(loopEnd - loopStart).count();
        cout << "Loop execution time: " << elapsed << " ms" << endl;
    }
    
    delete game;
    return 0;
}
