#include <SFML/Graphics.hpp>

int main()
{
  sf::RenderWindow window(sf::VideoMode(800, 600), "SFML Triangle");

  sf::ConvexShape triangle;
  triangle.setPointCount(3);
  triangle.setPoint(0, sf::Vector2f(400.f, 100.f));
  triangle.setPoint(1, sf::Vector2f(200.f, 500.f));
  triangle.setPoint(2, sf::Vector2f(600.f, 500.f));
  triangle.setFillColor(sf::Color::Green);

  while (window.isOpen())
  {
    sf::Event event;
    while (window.pollEvent(event))
    {
      if (event.type == sf::Event::Closed)
      {
        window.close();
      }
    }

    window.clear(sf::Color::Black);
    window.draw(triangle);
    window.display();
  }

  return 0;
}
