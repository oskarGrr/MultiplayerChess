#include "ChessApplication.h"
#include <exception>
#include <fstream>

int main(int argCount, char* arguments[])
{
    try
    {
        ChessApp& app = ChessApp::getApp();
        app.run();
    }
    catch(std::exception& e)
    {
        std::ofstream ofs("log.txt");
        ofs << e.what() << '\n';
        ofs.close();
        return EXIT_FAILURE;
    }
    catch(char const* msg)//for catching instances where I just throw a string message
    {
        std::ofstream ofs("log.txt");
        ofs << msg << '\n';
        ofs.close();
    }
    catch(...)
    {
        std::ofstream ofs("log.txt");
        ofs << "Unandled exception of unknown type"
            << "(not deriving from std::exception or a simple const char*)\n";
        ofs.close();
    }

    return EXIT_SUCCESS;
}