#include <iostream>
#include <string>
#include "command_handler.h"
#include "command_parser.h"

// Función temporal para pruebas en consola
void repl() {
    CommandHandler handler;
    std::string line;
    
    std::cout << "=== ExtreamFS - Sistema de Archivos EXT2 ===" << std::endl;
    std::cout << "Escribe comandos (exit para salir):" << std::endl;
    std::cout << std::endl;
    
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);
        
        if (line.empty()) continue;
        if (CommandParser::toLower(line) == "exit") break;
        
        std::string result = handler.processCommand(line);
        std::cout << result << std::endl;
    }
}

int main() {
    // Modo REPL para pruebas locales
    repl();
    
    return 0;
}
