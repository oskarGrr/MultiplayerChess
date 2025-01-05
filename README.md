
# A multiplayer capable chess game made in C++ with SDL2, imGui and the OS socket API.

---

## A quick overview:

The server source:\
[https://github.com/oskarGrr/chessServer](https://github.com/oskarGrr/chessServer)

I'm an avid chess player and programmer, so creating a chess app seemed like a natural choice.\
This chess app goes a above and beyond a lot of other chess desktop applications in a few ways:

* I taught myself TCP socket programming to add online capabilities, allowing players to compete against others over the internet.
* I wrote a multithreaded [server](https://github.com/oskarGrr/chessServer) in C that can handle serveral games/clients at once.
* I have used the [Dear ImGui](https://github.com/ocornut/imgui) GUI library to create a more visually appealing experience with a menu bar, Arrow drawing like on chess.com and lichess, a way to edit the colors of the squares, flipping the board to view it from the other players perspective, an aesthetically pleasing pawn promotion selection popup and more.
* I Implemented my own file serializer/deserializer to save settings in simple txt files
* The ability to request and reply to chess draw offers, resign and request a rematch against your online opponent.
* I Implemented [my own publisher subscriber event system](https://github.com/oskarGrr/EventSystem) to allow for code decoupling between different modules of the code (like between the core game logic and the GUI code). This helps to decouple the code but also enable easier extendability of different systems.
* A lot of thought went into the architecture of this code, resulting in numerous rounds of refactoring to enhance its readability and extensibility. For me, building projects with a focus on learning involves more than just creating lots of portfolio pieces. It also includes revisiting old projects and applying new insights to improve their code quality. For me, it's about quality more than quantity.

## Some future additions:
* The ability to specify more paramaters when sending a game offer to someone online. For instance, the ability to specify the [time control](https://www.chess.com/blog/RussBell/time-controls-everything-you-wanted-to-know).
* A history of moves on the right side of the main window with GUI buttons and keyboard buttons that allow the user to step back and forth through the history of the chess game to visualize the history of moves.
* Implementing things like the [50 move rule](https://en.wikipedia.org/wiki/Fifty-move_rule), [Threefold repition draw](https://en.wikipedia.org/wiki/Threefold_repetition), and ending the game in a draw automatically depending on the current material of the players.
* Different game modes like 4 player chess and atomic chess.
* Support for linux (I just need to slightly modify the networking code)

## Build instructions:
1) If you don't already have it, install CMake and vcpkg.
2) Make sure you have an environment called VCPKG_ROOT which is the path to your vcpkg installation 
    (on windows I had to use double backslashes for path seperators in VCPKG_ROOT to avoid a CMake bug)
3) Simply run generateProject.bat