# OOP-Project-RPG-Game
⚔️ RPG Engine: The Vroomstein Chronicles
A Custom SFML-Based RPG Built from Scratch

Tired of "pay-to-win" mechanics and uninspired sequels, this project is a ground-up development of a modular RPG engine. Developed for the 2nd Semester OOP curriculum, the game pits a player-selected Hero against the mechanical menace Dr. Vroomstein and his waves of polymorphic minions.  

🏗️ Core Architecture (OOP Implementation)
The engine is built on a strict modular hierarchy to ensure efficiency and scalability, avoiding shortcuts in favor of robust software design.

The Avatar Hierarchy
The foundation of every character in the game is the Avatar Abstract Class.  

Attributes: Every entity tracks a unique name, base speed, maximum health, and current health.  

Screen Constraints: Implements strict boundary checking to ensure no character escapes the viewport.  

Polymorphism: Derived classes for Hero, Villain, and Minions allow for specialized behaviors under a unified interface.  

Modular Moveset System
Instead of hardcoding attacks, the engine utilizes a dedicated Move class system.  

Dynamic Attributes: Every move features unique animations, damage values, and cooldown periods.  

Move Types:

Melee: High damage, limited range.  

Ranged: Moderate damage, long-distance reach.  

Special: Devastating damage with a significant recharge penalty.  

Input Mapping: Integrated with SFML handling to trigger moves via the S, D, and F keys.  

🎮 Gameplay Features
Hero Selection: Choose from three distinct heroes, each with unique sprites, health pools, base speeds, and customized movesets.  

The Boss (Dr. Vroomstein): A high-HP antagonist featuring a randomized attack pattern and the ability to summon reinforcements.  

Polymorphic Minions: Waves of enemies that move in structured formations (inspired by kinematic motion) and deal damage upon collision.  

Smart Scoring: A modular damage-dealing and pointing system designed to keep the gameplay loop engaging.  

🛠️ Technical Stack
Language: C++

Graphics/GUI: SFML (Simple and Fast Multimedia Library)  

Design Patterns: Abstract Base Classes, Composition (for Movesets), and Physics-based movement logic.  

🚀 How to Play
Select your Hero at the start screen.

Move through the environment while avoiding Dr. Vroomstein's minions.

Engage using your skills:

S — Melee Attack

D — Ranged Attack

F — Special Ability

Defeat the Boss to save the world from the mechanical menace!

Developed as a collaborative 2nd-semester project focusing on the intersection of Game Development and Object-Oriented Programming.
