#include "command_handler.h"
#include "command_parser.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdio>

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
    
    // Convertir a bytes - solo K y M, NO bytes
    std::string unitLower = CommandParser::toLower(unitStr);
    if (unitLower == "k") {
        size *= 1024;
    } else if (unitLower == "m") {
        size *= 1024 * 1024;
    } else {
        return "Error: Unidad no válida. Use K o M.";
    }
    
    // Validar fit - debe ser BF, FF o WF
    std::string fitLower = CommandParser::toLower(fitStr);
    char fit = 'F';
    if (fitLower == "bf" || fitLower == "b") {
        fit = 'B';
    } else if (fitLower == "ff" || fitLower == "f") {
        fit = 'F';
    } else if (fitLower == "wf" || fitLower == "w") {
        fit = 'W';
    } else {
        return "Error: Fit no válido. Use BF, FF o WF.";
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
    std::vector<std::string> mandatory = {"size", "path", "name"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string sizeStr = CommandParser::getParameter(params, "size");
    std::string path = CommandParser::getParameter(params, "path");
    std::string unitStr = CommandParser::getParameter(params, "unit", "K");
    std::string typeStr = CommandParser::getParameter(params, "type", "P");
    std::string fitStr = CommandParser::getParameter(params, "fit", "FF");
    std::string name = CommandParser::getParameter(params, "name");
    
    // Validar disco existe
    if (!DiskManager::fileExists(path)) {
        return "Error: El disco no existe.";
    }
    
    // Convertir tamaño
    int size;
    try {
        size = std::stoi(sizeStr);
    } catch (...) {
        return "Error: Tamaño inválido.";
    }
    
    if (size <= 0) {
        return "Error: Tamaño debe ser mayor que cero.";
    }
    
    // Convertir a bytes - solo K y M, NO bytes para FDISK
    std::string unitLower = CommandParser::toLower(unitStr);
    if (unitLower == "k") {
        size *= 1024;
    } else if (unitLower == "m") {
        size *= 1024 * 1024;
    } else {
        return "Error: Unidad no válida. Use K o M.";
    }
    
    // Validar tipo
    char partType = CommandParser::toLower(typeStr)[0];
    if (partType != 'p' && partType != 'e' && partType != 'l') {
        return "Error: Tipo inválido. Use P, E o L.";
    }
    
    // Validar fit - debe ser BF, FF o WF
    std::string fitLower = CommandParser::toLower(fitStr);
    char fitChar = 'F';  // Por defecto First Fit
    
    if (fitLower == "bf" || fitLower == "b") {
        fitChar = 'B';
    } else if (fitLower == "ff" || fitLower == "f") {
        fitChar = 'F';
    } else if (fitLower == "wf" || fitLower == "w") {
        fitChar = 'W';
    } else {
        return "Error: Fit no válido. Use BF, FF o WF.";
    }
    
    // Leer MBR
    int diskSize = (int)DiskManager::getFileSize(path);
    if (diskSize <= 0) {
        return "Error: No se pudo obtener tamaño del disco.";
    }
    
    MBR mbr;
    if (!DiskManager::readMBR(path, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Verificar nombre único
    if (DiskManager::findPartitionByName(path, name) != -1) {
        return "Error: Partición con ese nombre ya existe.";
    }
    
    // Validar límites de particiones
    int primaryCount = 0;
    int extendedCount = 0;
    int freeSlot = -1;
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_type == 'N') {
            if (freeSlot == -1) freeSlot = i;
        } else {
            if (mbr.mbr_partitions[i].part_type == 'P') primaryCount++;
            else if (mbr.mbr_partitions[i].part_type == 'E') extendedCount++;
        }
    }
    
    // Validaciones específicas
    if (partType == 'p' || partType == 'e') {
        if (freeSlot == -1) {
            return "Error: Se alcanzó el máximo de 4 particiones.";
        }
        if (partType == 'e' && extendedCount > 0) {
            return "Error: Solo se permite una partición extendida.";
        }
    } else if (partType == 'l') {
        if (extendedCount == 0) {
            return "Error: Debe haber una partición extendida para crear lógicas.";
        }
    }
    
    // Verificar espacio
    int freeSpace = DiskManager::getFreeSpace(path, diskSize);
    if (freeSpace < size) {
        return "Error: No hay suficiente espacio. Libre: " + std::to_string(freeSpace) + " bytes.";
    }
    
    // Crear partición primaria o extendida
    if (partType == 'p' || partType == 'e') {
        int startPos = DiskManager::calculatePartitionStart(path, size, fitChar, diskSize);
        if (startPos < 0) {
            return "Error: No se pudo encontrar espacio para la partición.";
        }
        
        Partition newPart;
        newPart.part_status = '0';
        newPart.part_type = (partType == 'p') ? 'P' : 'E';
        newPart.part_fit = fitChar;
        newPart.part_start = startPos;
        newPart.part_s = size;
        newPart.part_correlative = -1;
        std::strcpy(newPart.part_name, name.c_str());
        std::strcpy(newPart.part_id, "");
        
        mbr.mbr_partitions[freeSlot] = newPart;
        
        if (!DiskManager::writeMBR(path, mbr)) {
            return "Error: No se pudo escribir en el MBR.";
        }
        
        return "Partición " + name + " creada exitosamente.";
    } else {
        // TODO: Implementar particiones lógicas (EBR)
        return "Particiones lógicas: A implementar.";
    }
}

std::string CommandHandler::cmdMount(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"path", "name"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string path = CommandParser::getParameter(params, "path");
    std::string name = CommandParser::getParameter(params, "name");
    
    if (!DiskManager::fileExists(path)) {
        return "Error: El disco no existe.";
    }
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(path, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Buscar partición por nombre
    int partIndex = DiskManager::findPartitionByName(path, name);
    if (partIndex < 0) {
        return "Error: Partición no encontrada.";
    }
    
    // Validar que sea primaria
    if (mbr.mbr_partitions[partIndex].part_type != 'P') {
        return "Error: Solo se pueden montar particiones primarias.";
    }
    
    // Validar que no esté ya montada
    if (mbr.mbr_partitions[partIndex].part_status == '1') {
        return "Error: Partición ya está montada.";
    }
    
    // Generar ID: últimos 2 dígitos del carnet + número de partición + letra
    // Para este proyecto, usaremos números secuenciales como carnet simulado (062)
    int partitionNumber = 1;
    char letter = 'A';
    
    // Contar particiones montadas para saber la letra
    int mountedCount = 0;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_status == '1') {
            mountedCount++;
        }
    }
    
    letter = 'A' + mountedCount;
    partitionNumber = mountedCount + 1;
    
    char idStr[5];
    sprintf(idStr, "%d%c", partitionNumber, letter);
    
    // Actualizar partición
    mbr.mbr_partitions[partIndex].part_status = '1';
    mbr.mbr_partitions[partIndex].part_correlative = partitionNumber;
    std::strcpy(mbr.mbr_partitions[partIndex].part_id, idStr);
    
    if (!DiskManager::writeMBR(path, mbr)) {
        return "Error: No se pudo actualizar el MBR.";
    }
    
    // Guardar en particiones montadas
    mountedPartitions[std::string(idStr)] = path;
    
    return "Partición montada con ID: " + std::string(idStr);
}

std::string CommandHandler::cmdMounted(const std::map<std::string, std::string>& params) {
    if (mountedPartitions.empty()) {
        return "No hay particiones montadas.";
    }
    
    std::string result = "Particiones montadas: ";
    for (const auto& pair : mountedPartitions) {
        result += pair.first + ", ";
    }
    
    // Remover última coma
    if (result.length() > 2) {
        result = result.substr(0, result.length() - 2);
    }
    
    return result;
}

std::string CommandHandler::cmdMkfs(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"id", "type"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string id = CommandParser::getParameter(params, "id");
    std::string typeStr = CommandParser::getParameter(params, "type");
    
    // Validar que type sea FULL
    if (CommandParser::toLower(typeStr) != "full") {
        return "Error: Type debe ser FULL.";
    }
    
    // Buscar partición montada
    if (mountedPartitions.find(id) == mountedPartitions.end()) {
        return "Error: Partición no montada. ID: " + id;
    }
    
    std::string diskPath = mountedPartitions[id];
    
    // Leer MBR para encontrar partición
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición por ID
    Partition* part = nullptr;
    int partIndex = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == id) {
            part = &mbr.mbr_partitions[i];
            partIndex = i;
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Calcular número de inodos y bloques
    int partSize = part->part_s;
    int superblockSize = sizeof(Superblock);
    
    // Fórmula: tamaño_particion = sizeof(superblock) + n + 3*n + n*sizeof(inodos) + 3*n*sizeof(block)
    // Donde n es el número a despejar
    // Simplificamos: partSize = superblockSize + n + 3*n + n*64 + 3*n*64
    // partSize = superblockSize + n(1 + 3 + 64 + 192)
    // partSize = superblockSize + 260*n
    
    int numerator = partSize - superblockSize;
    int denominator = 260;
    int numInodes = numerator / denominator;
    int numBlocks = numInodes * 3;
    
    if (numInodes <= 0) {
        return "Error: Partición muy pequeña para EXT2.";
    }
    
    // Crear superblock
    Superblock sb;
    sb.s_filesystem_type = 2;
    sb.s_inodes_count = numInodes;
    sb.s_blocks_count = numBlocks;
    sb.s_free_blocks_count = numBlocks;
    sb.s_free_inodes_count = numInodes;
    sb.s_mtime = time(nullptr);
    sb.s_umtime = time(nullptr);
    sb.s_mnt_count = 0;
    sb.s_magic = 0xEF53;
    sb.s_inode_s = sizeof(Inodo);
    sb.s_block_s = 64;
    sb.s_firts_ino = 0;
    sb.s_first_blo = 0;
    
    // Calcular posiciones
    int bitmapInodeStart = part->part_start + superblockSize;
    int bitmapBlockStart = bitmapInodeStart + ((numInodes + 7) / 8);
    int inodeStart = bitmapBlockStart + ((numBlocks + 7) / 8);
    int blockStart = inodeStart + (numInodes * sizeof(Inodo));
    
    sb.s_bm_inode_start = bitmapInodeStart;
    sb.s_bm_block_start = bitmapBlockStart;
    sb.s_inode_start = inodeStart;
    sb.s_block_start = blockStart;
    
    // Escribir superblock
    if (!DiskManager::writeToDisk(diskPath, part->part_start, (char*)&sb, sizeof(Superblock))) {
        return "Error: No se pudo escribir el superblock.";
    }
    
    // Inicializar bitmaps (todos en 0 = libres)
    char zeroBitmap[512];
    std::memset(zeroBitmap, 0, sizeof(zeroBitmap));
    
    DiskManager::writeToDisk(diskPath, bitmapInodeStart, zeroBitmap, (numInodes + 7) / 8);
    DiskManager::writeToDisk(diskPath, bitmapBlockStart, zeroBitmap, (numBlocks + 7) / 8);
    
    // TODO: Crear archivo users.txt en la raíz
    
    return "Partición formateada con EXT2. Inodos: " + std::to_string(numInodes) + 
           ", Bloques: " + std::to_string(numBlocks);
}

std::string CommandHandler::cmdLogin(const std::map<std::string, std::string>& params) {
    std::vector<std::string> mandatory = {"user", "pass", "id"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    if (isLoggedIn) {
        return "Error: Ya hay una sesión activa. Use logout primero.";
    }
    
    std::string user = CommandParser::getParameter(params, "user");
    std::string pass = CommandParser::getParameter(params, "pass");
    std::string id = CommandParser::getParameter(params, "id");
    
    // TODO: Validar usuario y contraseña en users.txt (case-sensitive)
    // Por ahora, aceptamos cualquier usuario/pass válido
    currentUser = user;
    currentPartitionId = id;
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
    // Buscar parámetros -file1, -file2, -file3, etc.
    std::string result = "";
    int fileNum = 1;
    
    while (true) {
        std::string fileParam = "-file" + std::to_string(fileNum);
        if (params.find(CommandParser::toLower(fileParam.substr(1))) == params.end()) {
            break;  // No hay más archivos
        }
        
        std::string filePath = CommandParser::getParameter(params, fileParam.substr(1));
        
        if (filePath.empty()) {
            return "Error: Archivo " + std::to_string(fileNum) + " no especificado.";
        }
        
        if (!isLoggedIn) {
            return "Error: Debe iniciar sesión.";
        }
        
        // TODO: Implementar lectura real de archivo desde inodos
        // Por ahora, solo verificamos que sea un formato válido
        result += "Contenido de " + filePath + ": [A implementar]\n";
        
        fileNum++;
    }
    
    if (fileNum == 1) {
        return "Error: No se especificaron archivos.";
    }
    
    return result;
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
