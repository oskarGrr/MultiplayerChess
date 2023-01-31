#include "ChessApplication.h"
#include <exception>//std::exception
#include <fstream>//std::ofstream

int main(int argCount, char** arguments)
{    
    try
    {
        ChessApp& app = ChessApp::getApp();
        app.run();
    }
    catch(std::exception& e)
    {
        std::ofstream ofs("log.txt", std::ios_base::app);
        ofs << "exception thrown at "
            << ChessApp::getCurrentDateAndTime() << ": " 
            << e.what() << "\n\n\n";
        return EXIT_FAILURE;
    }
    catch(...)
    {
        std::ofstream ofs("log.txt", std::ios_base::app);
        ofs << "exception of unknown type thrown at " 
            << ChessApp::getCurrentDateAndTime();
    }

    return EXIT_SUCCESS;
}