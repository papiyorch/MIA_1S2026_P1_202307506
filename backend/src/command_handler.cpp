#include "command_handler.h"
#include "command_parser.h"
#include <iostream>
#include <sstream>
#include <algorithm>

CommandHandler::CommandHandler() : currentUser(""), currentPartitionId(""), isLoggedIn(false) {}

std::string CommandHandler::processCommand(const std::string& line) {
    std::string commandName = CommandParser::getCommandName(line);
    params = CommandParser::parseCommand(line);
    
    // Comandos de disco (no requieren login)
    if (commandName == "mkdisk") return cmdMkdisk(params);
    if (commandName == "rmdisk") return cmdRmdisk(params);
    if (commandName == "fdisk") return cmdFdisk(params);
    if (commandName == "mount") return cmdMount(params);
    if (commandName == "mounted") return cmdMounted(params);
    
    // Comando de formateo y login (no requieren sesión activa)
    if (commandName == "mkfs") return cmdMkfs(params);
    if (commandName == "login") return cmdLogin(params);
    if (commandName == "logout") return cmdLogout(params);
    
    // Resto de comandos requieren sesión activa
    if (!isLoggedIn && commandName != "mkfs" && commandName != "login") {
        return "Error: Debe iniciar sesión primero.";
    }
    
    if (commandName == "mkgrp") return cmdMkgrp(params);
    if (commandName == "rmgrp") return cmdRmgrp(params);
    if (commandName == "mkusr") return cmdMkusr(params);
    if (commandName == "rmusr") return cmdRmusr(params);
    if (commandName == "chgrp") return cmdChgrp(params);
    if (commandName == "mkdir") return cmdMkdir(params);
    if (commandName == "mkfile") return cmdMkfile(params);
    if (commandName == "cat") return cmdCat(params);
    if (commandName == "rep") return cmdRep(params);
    
    return "Error: Comando no reconocido: " + commandName;
}

std::string CommandHandler::getCurrentUser() const {
    return currentUser;
}

std::string CommandHandler::getCurrentPartitionId() const {
    return currentPartitionId;
}

std::string CommandHandler::cmdMkdisk(const std::map<std::string, std::string>& params) {
    // Parámetros obligatorios: -size, -path
    std::vector<std::string> mandatory = {"size", "path"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    // Obtener parámetros
    std::string sizeStr = CommandParser::getParameter(params, "size");
    std::string path = CommandParser::getParameter(params, "path");
    std::string unitStr = CommandParser::getParameter(params, "unit", "K");
    std::string fitStr = CommandParser::getParameter(params, "fit", "FF");
    
    // Validar y convertir tamaño
    int size;
    try {
        size = std::stoi(sizeStr);
    } catch (...) {
        return "Error: Tamaño inválido.";
    }
    
    if (size <= 0) {
        return "Error: Tamaño debe ser mayor que cero.";
    }
    
    // Convertir a bytes
    if (CommandParser::toLower(unitStr) == "k") {
        size *= 1024;
    } else if (CommandParser::toLower(unitStr) == "m") {
        size *= 1024 * 1024;
    } else if (CommandParser::toLower(unitStr) != "b") {
        return "Error: Unidad no válida. Use B, K o M.";
    }
    
    // Validar fit
    char fit = CommandParser::toLower(fitStr)[0];
    if (fit != 'BF' && fit != 'FF' && fit != 'WF') {
        return "Error: Tipo de ajuste no válido. Use BF, FF o WF.";
    }
    
    // Crear disco
    if (DiskManager::createDisk(path, size, fit)) {
        return "Disco creado exitosamente en: " + path;
    } else {
        return "Error: No se pudo crear el disco.";
    }
}

std::string CommandHandler::cmdRmdisk(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"path"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string path = CommandParser::getParameter(params, "path");
    
    if (DiskManager::removeDisk(path)) {
        return "Disco eliminado: " + path;
    } else {
        return "Error: No se pudo eliminar el disco.";
    }
}

std::string CommandHandler::cmdFdisk(const std::map<std::string, std::string>& params) {
    // TODO: Implementar FDISK completo
    return "FDISK: A implementar";
}

std::string CommandHandler::cmdMount(const std::map<std::string, std::string>& params) {
    // TODO: Implementar MOUNT completo
    return "MOUNT: A implementar";
}

std::string CommandHandler::cmdMounted(const std::map<std::string, std::string>& params) {
    // TODO: Implementar MOUNTED completo
    return "MOUNTED: A implementar";
}

std::string CommandHandler::cmdMkfs(const std::map<std::string, std::string>& params) {
    // TODO: Implementar MKFS completo
    return "MKFS: A implementar";
}

std::string CommandHandler::cmdLogin(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"user", "pass", "id"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    if (isLoggedIn) {
        return "Error: Ya hay una sesión activa. Use logout primero.";
    }
    
    // TODO: Validar usuario y contraseña en users.txt
    currentUser = CommandParser::getParameter(params, "user");
    currentPartitionId = CommandParser::getParameter(params, "id");
    isLoggedIn = true;
    
    return "Sesión iniciada: " + currentUser;
}

std::string CommandHandler::cmdLogout(const std::map<std::string, std::string>& params) {
    if (!isLoggedIn) {
        return "Error: No hay sesión activa.";
    }
    
    currentUser = "";
    currentPartitionId = "";
    isLoggedIn = false;
    
    return "Sesión cerrada.";
}

std::string CommandHandler::cmdCat(const std::map<std::string, std::string>& params) {
    // TODO: Implementar CAT completo
    return "CAT: A implementar";
}

std::string CommandHandler::cmdMkgrp(const std::map<std::string, std::string>& params) {
    // TODO: Implementar MKGRP completo
    return "MKGRP: A implementar";
}

std::string CommandHandler::cmdRmgrp(const std::map<std::string, std::string>& params) {
    // TODO: Implementar RMGRP completo
    return "RMGRP: A implementar";
}

std::string CommandHandler::cmdMkusr(const std::map<std::string, std::string>& params) {
    // TODO: Implementar MKUSR completo
    return "MKUSR: A implementar";
}

std::string CommandHandler::cmdRmusr(const std::map<std::string, std::string>& params) {
    // TODO: Implementar RMUSR completo
    return "RMUSR: A implementar";
}

std::string CommandHandler::cmdChgrp(const std::map<std::string, std::string>& params) {
    // TODO: Implementar CHGRP completo
    return "CHGRP: A implementar";
}

std::string CommandHandler::cmdMkdir(const std::map<std::string, std::string>& params) {
    // TODO: Implementar MKDIR completo
    return "MKDIR: A implementar";
}

std::string CommandHandler::cmdMkfile(const std::map<std::string, std::string>& params) {
    // TODO: Implementar MKFILE completo
    return "MKFILE: A implementar";
}

std::string CommandHandler::cmdRep(const std::map<std::string, std::string>& params) {
    // TODO: Implementar REP completo
    return "REP: A implementar";
}

std::string CommandHandler::validateMandatoryParams(const std::map<std::string, std::string>& params,
                                                   const std::vector<std::string>& mandatory) {
    for (const auto& param : mandatory) {
        if (!CommandParser::hasParameter(params, param)) {
            return "Error: Parámetro obligatorio faltante: -" + param;
        }
    }
    return "";
}
