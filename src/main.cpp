#include <SFML/Graphics.hpp>
#include <iostream>

int main()
{
    using namespace std;
    
    cout<<"Hello world"<<endl;
    cout<<"hi I am Hassaan Sajid"<<endl;
    
    sf::RenderWindow window(sf::VideoMode({200, 200}), "SFML works!");
    sf::CircleShape shape(100.f);
    shape.setFillColor(sf::Color::Green);

    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        window.clear();
        window.draw(shape);
        window.display();
    }

    return 0;
}
