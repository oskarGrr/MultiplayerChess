#include "ChessApplication.h"
#include "errorLogger.h"
#include <exception>//std::exception

int main(int argCount, char** arguments)
{    
    try
    {
        ChessApp& app = ChessApp::getApp();
        app.run();
    }
    catch(std::exception& e)
    {
        std::string errMsg{e.what()};
        errMsg.append(" caught in main()");
        FileErrorLogger::get().logErrors(errMsg);
        return EXIT_FAILURE;
    }
    catch(...)
    {
        FileErrorLogger::get().logErrors("exception caught of unknown type in main()");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}