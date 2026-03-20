#include "command_handler.h"
#include "command_parser.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <functional>
#include <set>

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
    std::string unitStr = CommandParser::getParameter(params, "unit", "M");
    std::string fitStr = CommandParser::getParameter(params, "fit", "FF");
    
    // Validar que la ruta termine en .mia
    if (path.length() < 4 || path.substr(path.length() - 4) != ".mia") {
        return "Error: El archivo debe tener extensión .mia";
    }
    
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
    
    // Convertir a bytes - solo K y M
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
    
    // Verificar si el archivo existe
    std::ifstream file(path);
    if (!file.good()) {
        return "Error: El archivo no existe: " + path;
    }
    file.close();
    
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
    std::string fitStr = CommandParser::getParameter(params, "fit", "WF");  // Default: WF (Worst Fit)
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
    
    // Convertir a bytes - K, M o B (default K)
    std::string unitLower = CommandParser::toLower(unitStr);
    if (unitLower == "k") {
        size *= 1024;
    } else if (unitLower == "m") {
        size *= 1024 * 1024;
    } else if (unitLower == "b") {
        // Ya está en bytes, no hacer nada
    } else {
        return "Error: Unidad no válida. Use B, K o M.";
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
        if (primaryCount + extendedCount >= 4) {
            return "Error: No puede haber más de 4 particiones (primarias + extendida). Actuales: " + 
                   std::to_string(primaryCount + extendedCount);
        }
        if (partType == 'e' && extendedCount > 0) {
            return "Error: Solo se permite una partición extendida por disco. Ya existe una.";
        }
        // Obtener slot libre (ya validamos que existe arriba)
        if (freeSlot == -1) {
            return "Error: No hay slots libres en la tabla de particiones.";
        }
    } else if (partType == 'l') {
        if (extendedCount == 0) {
            return "Error: Debe haber una partición extendida para crear particiones lógicas.";
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
        
        // Si es partición extendida, crear el primer EBR
        if (partType == 'e') {
            EBR ebr;
            ebr.part_mount = '0';
            ebr.part_fit = fitChar;
            ebr.part_start = -1;
            ebr.part_s = 0;
            ebr.part_next = -1;
            std::strcpy(ebr.part_name, "");
            
            if (!DiskManager::writeToDisk(path, newPart.part_start, (char*)&ebr, sizeof(EBR))) {
                return "Error: No se pudo crear EBR inicial para partición extendida.";
            }
        }
        
        return "Partición " + name + " creada exitosamente.";
    } else {
        // Particiones lógicas: solo se permite crear si hay una extendida
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
    
    // Determinar número de partición y letra para este disco
    int partitionNumber = 1;
    char letter = 'A';
    
    // Si ya hay montajes de este disco, incrementar numero y mantener letra
    // Si es otro disco, incrementar letra y reiniciar numero
    if (diskMountStates.find(path) != diskMountStates.end()) {
        // Este disco ya tiene montajes
        partitionNumber = diskMountStates[path] + 1;
        letter = diskLetterStates[path];
    } else {
        // Primer montaje de este disco
        // Contar discos ya montados para determinar la letra
        char maxLetter = 'A';
        for (const auto& pair : diskLetterStates) {
            maxLetter = std::max(maxLetter, pair.second);
        }
        
        // Si ya hay otros discos montados, usar la siguiente letra
        if (diskLetterStates.size() > 0) {
            letter = maxLetter + 1;
        } else {
            letter = 'A';
        }
        partitionNumber = 1;
        diskLetterStates[path] = letter;
    }
    
    // Incrementar contador para este disco
    diskMountStates[path] = partitionNumber;
    diskLetterStates[path] = letter;
    
    // Generar ID con formato: 06 (carnet) + número + letra
    char idStr[10];
    sprintf(idStr, "06%d%c", partitionNumber, letter);
    
    // Actualizar partición en memoria (NO escribir a disco)
    mbr.mbr_partitions[partIndex].part_status = '1';
    mbr.mbr_partitions[partIndex].part_correlative = partitionNumber;
    std::strcpy(mbr.mbr_partitions[partIndex].part_id, idStr);
    
    // Guardar en particiones montadas
    mountedPartitions[std::string(idStr)] = path;
    
    // Actualizar Superblock: incrementar s_mnt_count y s_mtime
    Superblock sb;
    if (DiskManager::readSuperblock(path, mbr.mbr_partitions[partIndex].part_start, sb)) {
        sb.s_mnt_count++;
        sb.s_mtime = time(nullptr);
        DiskManager::writeSuperblock(path, mbr.mbr_partitions[partIndex].part_start, sb);
    }
    
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
    std::vector<std::string> mandatory = {"id"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string id = CommandParser::getParameter(params, "id");
    std::string typeStr = CommandParser::getParameter(params, "type", "full"); 
    
    // Validar que type sea FULL (case-insensitive)
    if (CommandParser::toLower(typeStr) != "full") {
        return "Error: Type debe ser 'full'. Valor recibido: " + typeStr;
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
    int inodoSize = sizeof(Inodo);
    int blockSize = 64; 
    
    int numerator = partSize - superblockSize;
    int denominator = 4 + inodoSize + 3 * blockSize;  
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
    // Calcular tamaño necesario para cada bitmap
    int bitmapInodeSize = (numInodes + 7) / 8;
    int bitmapBlockSize = (numBlocks + 7) / 8;
    
    // Usar buffer dinámico para bitmaps
    std::vector<char> zeroBitmap(std::max(bitmapInodeSize, bitmapBlockSize), 0);
    
    DiskManager::writeToDisk(diskPath, bitmapInodeStart, zeroBitmap.data(), bitmapInodeSize);
    DiskManager::writeToDisk(diskPath, bitmapBlockStart, zeroBitmap.data(), bitmapBlockSize);
    
    // Crear directorio raíz (inodo 0)
    Inodo rootIno;
    rootIno.i_uid = 0;
    rootIno.i_gid = 0;
    rootIno.i_s = 0;
    rootIno.i_atime = time(nullptr);
    rootIno.i_ctime = time(nullptr);
    rootIno.i_mtime = time(nullptr);
    rootIno.i_type = 0;  // 0 = carpeta (directorio)
    std::strncpy(rootIno.i_perm, "755", 2);
    
    // Asignar primer bloque a la raíz
    int rootBlock = DiskManager::allocateBlock(diskPath, part->part_start, sb);
    if (rootBlock < 0) {
        return "Error: No se pudo asignar bloque para raíz.";
    }
    
    // Inicializar bloque raíz 
    BlockFolder rootBlockContent;
    // Primer registro: 
    std::strncpy(rootBlockContent.b_content[0].b_name, ".", 11);
    rootBlockContent.b_content[0].b_inodo = 0;
    
    // Segundo registro: 
    std::strncpy(rootBlockContent.b_content[1].b_name, "..", 11);
    rootBlockContent.b_content[1].b_inodo = 0;
    
    // Inicializar registros restantes como vacíos
    rootBlockContent.b_content[2].b_inodo = 0;
    rootBlockContent.b_content[3].b_inodo = 0;
    std::memset(rootBlockContent.b_content[2].b_name, 0, 12);
    std::memset(rootBlockContent.b_content[3].b_name, 0, 12);
    
    // Escribir bloque raíz
    DiskManager::writeBlock(diskPath, part->part_start, rootBlock, (char*)&rootBlockContent, sizeof(BlockFolder));
    
    for (int i = 0; i < 15; i++) {
        rootIno.i_block[i] = (i == 0) ? rootBlock : -1;
    }
    
    // Marcar inodo raíz como usado
    DiskManager::setBitmapBit(diskPath, sb.s_bm_inode_start, 0, true);
    sb.s_free_inodes_count--;
    
    // Escribir inodo raíz
    DiskManager::writeInodo(diskPath, part->part_start, 0, rootIno);
    
    // Escribir superblock actualizado
    DiskManager::writeSuperblock(diskPath, part->part_start, sb);
    
    // Crear archivo users.txt en la raíz
    int usersInode = DiskManager::allocateInode(diskPath, part->part_start, sb);
    if (usersInode < 0) {
        return "Partición formateada pero no se pudo crear users.txt";
    }
    
    // Asignar bloque para users.txt
    int usersBlock = DiskManager::allocateBlock(diskPath, part->part_start, sb);
    if (usersBlock < 0) {
        DiskManager::deallocateInode(diskPath, part->part_start, usersInode, sb);
        return "Partición formateada pero no se pudo crear users.txt";
    }
    
    // Crear contenido de users.txt
    std::string usersContent = "1,G,root\n1,U,root,root,123\n";
    BlockFile usersFileBlock;
    std::memset(usersFileBlock.b_content, 0, sizeof(usersFileBlock.b_content));
    std::memcpy(usersFileBlock.b_content, usersContent.c_str(), std::min((int)usersContent.length(), 64));
    
    // Escribir bloque del archivo
    DiskManager::writeBlock(diskPath, part->part_start, usersBlock, (char*)&usersFileBlock, sizeof(BlockFile));
    
    // Crear inodo para users.txt
    Inodo usersFileIno;
    usersFileIno.i_uid = 1;   
    usersFileIno.i_gid = 1;   
    usersFileIno.i_s = usersContent.length();
    usersFileIno.i_atime = time(nullptr);
    usersFileIno.i_ctime = time(nullptr);
    usersFileIno.i_mtime = time(nullptr);
    usersFileIno.i_type = 1; 
    std::strncpy(usersFileIno.i_perm, "600", 3);  
    
    for (int i = 0; i < 15; i++) {
        usersFileIno.i_block[i] = (i == 0) ? usersBlock : -1;
    }
    
    // Escribir inodo de users.txt
    DiskManager::writeInodo(diskPath, part->part_start, usersInode, usersFileIno);
    
    // Marcar inodo como usado
    DiskManager::setBitmapBit(diskPath, sb.s_bm_inode_start, usersInode, true);
    sb.s_free_inodes_count--;
    
    // Agregar entrada users.txt al bloque raíz existente
    rootBlockContent.b_content[2].b_inodo = usersInode;
    std::strncpy(rootBlockContent.b_content[2].b_name, "users.txt", 11);
    
    // Escribir bloque raíz actualizado
    DiskManager::writeBlock(diskPath, part->part_start, rootBlock, (char*)&rootBlockContent, sizeof(BlockFolder));
    
    // Escribir superblock final actualizado
    DiskManager::writeSuperblock(diskPath, part->part_start, sb);
    
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
    
    // Validar que la partición está montada
    if (mountedPartitions.find(id) == mountedPartitions.end()) {
        return "Error: Partición no montada con ID: " + id;
    }
    
    std::string diskPath = mountedPartitions[id];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición
    Partition* part = nullptr;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == id) {
            part = &mbr.mbr_partitions[i];
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                   (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear users.txt y buscar usuario
    std::istringstream iss(usersContent);
    std::string line;
    bool userFound = false;
    bool passwordCorrect = false;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        // Parsear línea: ID,Tipo,Grupo/Usuario,Usuario,Contraseña
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            fields.push_back(field);
        }
        
        // Solo procesar líneas de usuario (tipo U)
        if (fields.size() >= 5 && fields[1] == "U") {

            if (fields[3] == user) {
                userFound = true;
                if (fields[4] == pass) {
                    passwordCorrect = true;
                    break;
                }
            }
        }
    }
    
    if (!userFound) {
        return "Error: Usuario no encontrado: " + user;
    }
    
    if (!passwordCorrect) {
        return "Error: Contraseña incorrecta.";
    }
    
    // Todos los datos válidos, establecer sesión
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
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: No hay partición montada.";
    }
    
    std::string result = "";
    int fileNum = 1;
    
    while (true) {
        std::string fileParam = "file" + std::to_string(fileNum);
        if (params.find(fileParam) == params.end()) {
            break;
        }
        
        std::string filePath = CommandParser::getParameter(params, fileParam);
        if (filePath.empty()) {
            return "Error: Archivo " + std::to_string(fileNum) + " no especificado.";
        }
        
        // Extraer nombre del archivo de la ruta (ej: /home/a.txt -> a.txt)
        std::string fileName = filePath;
        size_t lastSlash = filePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            fileName = filePath.substr(lastSlash + 1);
        }
        
        // Buscar partición montada
        std::string diskPath = mountedPartitions[currentPartitionId];
        
        // Leer MBR
        MBR mbr;
        if (!DiskManager::readMBR(diskPath, mbr)) {
            return "Error: No se pudo leer el MBR.";
        }
        
        // Encontrar partición
        Partition* part = nullptr;
        for (int i = 0; i < 4; i++) {
            if (std::string(mbr.mbr_partitions[i].part_id) == currentPartitionId) {
                part = &mbr.mbr_partitions[i];
                break;
            }
        }
        
        if (part == nullptr) {
            return "Error: Partición no encontrada.";
        }
        
        // Leer superblock
        Superblock sb;
        if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
            return "Error: No se pudo leer el superblock.";
        }
        
        // Buscar archivo en el directorio raíz
        BlockFolder rootBlock;
        if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
            return "Error: No se pudo leer el directorio raíz.";
        }
        
        int fileInode = -1;
        for (int i = 0; i < 4; i++) {
            if (std::string(rootBlock.b_content[i].b_name) == fileName) {
                fileInode = rootBlock.b_content[i].b_inodo;
                break;
            }
        }
        
        if (fileInode < 0) {
            return "Error: Archivo no encontrado: " + fileName;
        }
        
        // Leer inodo del archivo
        Inodo fileIno;
        if (!DiskManager::readInodo(diskPath, part->part_start, fileInode, fileIno)) {
            return "Error: No se pudo leer el inodo del archivo.";
        }
        
        if (fileIno.i_type != 1) {
            return "Error: No es un archivo: " + fileName;
        }
        
        // Validar permisos de lectura 
        int permNum = std::stoi(std::string(fileIno.i_perm));
        int otherPerm = permNum % 10;  
        
        // Validar que otros puedan leer
        if ((otherPerm & 4) == 0) {  
            return "Error: Permiso denegado. No tiene acceso de lectura: " + fileName;
        }
        
        // Leer contenido del archivo
        for (int i = 0; i < 12 && fileIno.i_block[i] >= 0; i++) {
            BlockFile block;
            int blockSize = std::min(64, fileIno.i_s - (i * 64));
            if (!DiskManager::readBlock(diskPath, part->part_start, fileIno.i_block[i], 
                                       (char*)&block, blockSize)) {
                return "Error: No se pudo leer bloque del archivo.";
            }
            result.append(block.b_content, blockSize);
        }
        
        if (fileNum < 2) {
            result += "\n";
        } else {
            result += "\n";
        }
        fileNum++;
    }
    
    if (fileNum == 1) {
        return "Error: No se especificaron archivos.";
    }
    
    // Remover última línea en blanco si existe
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result;
}

std::string CommandHandler::cmdMkgrp(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root (UID = 1)
    if (currentUser != "root") {
        return "Error: Solo root puede crear grupos.";
    }
    
    std::vector<std::string> mandatory = {"name"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string groupName = CommandParser::getParameter(params, "name");
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición
    Partition* part = nullptr;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == currentPartitionId) {
            part = &mbr.mbr_partitions[i];
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                   (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y buscar si el grupo ya existe
    std::istringstream iss(usersContent);
    std::string line;
    int maxId = 1;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        if (fields.size() >= 3) {
            // Actualizar maxId
            try {
                int id = std::stoi(fields[0]);
                if (id > maxId) maxId = id;
            } catch (...) {}
            
            // Verificar si es el grupo que intentamos crear
            if (fields[1] == "G" && fields.size() >= 3 && fields[2] == groupName) {
                return "Error: Grupo ya existe: " + groupName;
            }
        }
    }
    
    // Crear nueva línea para el grupo
    int newGroupId = maxId + 1;
    std::string newGroupLine = std::to_string(newGroupId) + ", G, " + groupName + "\n";
    usersContent += newGroupLine;
    
    // Escribir nuevo contenido a users.txt
    // Actualizar tamaño del inodo
    usersIno.i_s = usersContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < usersContent.length()) {
        int blockSize = std::min(64, (int)(usersContent.length() - contentOffset));
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, usersContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Grupo " + groupName + " creado exitosamente.";
}

std::string CommandHandler::cmdRmgrp(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root
    if (currentUser != "root") {
        return "Error: Solo root puede eliminar grupos.";
    }
    
    std::vector<std::string> mandatory = {"name"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string groupName = CommandParser::getParameter(params, "name");
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición
    Partition* part = nullptr;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == currentPartitionId) {
            part = &mbr.mbr_partitions[i];
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                   (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y buscar el grupo a eliminar
    std::istringstream iss(usersContent);
    std::string line;
    std::string newContent = "";
    bool groupFound = false;
    
    while (std::getline(iss, line)) {
        if (line.empty()) {
            newContent += "\n";
            continue;
        }
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        // Verificar si es el grupo que queremos eliminar
        if (fields.size() >= 3 && fields[1] == "G" && fields[2] == groupName) {
            groupFound = true;
            // Marcar como eliminado (0 en lugar del ID)
            newContent += "0, G, " + groupName + "\n";
        } else {
            newContent += line + "\n";
        }
    }
    
    if (!groupFound) {
        return "Error: Grupo no encontrado: " + groupName;
    }
    
    // Remover última línea en blanco si existe
    if (!newContent.empty() && newContent.back() == '\n') {
        newContent.pop_back();
    }
    
    // Actualizar tamaño del inodo
    usersIno.i_s = newContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < newContent.length()) {
        int blockSize = std::min(64, (int)(newContent.length() - contentOffset));
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, newContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Grupo " + groupName + " eliminado exitosamente.";
}

std::string CommandHandler::cmdMkusr(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root
    if (currentUser != "root") {
        return "Error: Solo root puede crear usuarios.";
    }
    
    std::vector<std::string> mandatory = {"user", "pass", "grp"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string userName = CommandParser::getParameter(params, "user");
    std::string password = CommandParser::getParameter(params, "pass");
    std::string groupName = CommandParser::getParameter(params, "grp");
    
    // Validar longitud de parámetros
    if (userName.length() > 10) {
        return "Error: El nombre de usuario no puede exceder 10 caracteres.";
    }
    
    if (password.length() > 10) {
        return "Error: La contraseña no puede exceder 10 caracteres.";
    }
    
    if (groupName.length() > 10) {
        return "Error: El nombre del grupo no puede exceder 10 caracteres.";
    }
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición
    Partition* part = nullptr;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == currentPartitionId) {
            part = &mbr.mbr_partitions[i];
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                   (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y validar
    std::istringstream iss(usersContent);
    std::string line;
    int maxId = 1;
    bool userExists = false;
    bool groupExists = false;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        if (fields.size() >= 3) {
            try {
                int id = std::stoi(fields[0]);
                if (id > maxId) maxId = id;
            } catch (...) {}
            
            // Verificar si el usuario ya existe
            if (fields[1] == "U" && fields.size() >= 4 && fields[3] == userName) {
                userExists = true;
            }
            
            // Verificar si el grupo existe y está activo
            if (fields[1] == "G" && fields.size() >= 3 && fields[2] == groupName) {
                try {
                    int id = std::stoi(fields[0]);
                    if (id > 0) {  // Activo (no eliminado)
                        groupExists = true;
                    }
                } catch (...) {}
            }
        }
    }
    
    if (userExists) {
        return "Error: Usuario ya existe: " + userName;
    }
    
    if (!groupExists) {
        return "Error: Grupo no existe o está inactivo: " + groupName;
    }
    
    // Crear nueva línea para el usuario
    int newUserId = maxId + 1;
    std::string newUserLine = std::to_string(newUserId) + ", U, " + groupName + ", " + userName + ", " + password + "\n";
    usersContent += newUserLine;
    
    // Actualizar tamaño del inodo
    usersIno.i_s = usersContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < usersContent.length()) {
        int blockSize = std::min(64, (int)(usersContent.length() - contentOffset));
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, usersContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Usuario " + userName + " creado en grupo " + groupName + " exitosamente.";
}

std::string CommandHandler::cmdRmusr(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root
    if (currentUser != "root") {
        return "Error: Solo root puede eliminar usuarios.";
    }
    
    std::vector<std::string> mandatory = {"user"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string userName = CommandParser::getParameter(params, "user");
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición
    Partition* part = nullptr;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == currentPartitionId) {
            part = &mbr.mbr_partitions[i];
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                   (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y buscar el usuario a eliminar
    std::istringstream iss(usersContent);
    std::string line;
    std::string newContent = "";
    bool userFound = false;
    
    while (std::getline(iss, line)) {
        if (line.empty()) {
            newContent += "\n";
            continue;
        }
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        // Verificar si es el usuario que queremos eliminar
        if (fields.size() >= 4 && fields[1] == "U" && fields[3] == userName) {
            userFound = true;
            // Marcar como eliminado 
            newContent += "0, U, " + fields[2] + ", " + fields[3] + ", " + fields[4] + "\n";
        } else {
            newContent += line + "\n";
        }
    }
    
    if (!userFound) {
        return "Error: Usuario no encontrado: " + userName;
    }
    
    // Remover última línea en blanco si existe
    if (!newContent.empty() && newContent.back() == '\n') {
        newContent.pop_back();
    }
    
    // Actualizar tamaño del inodo
    usersIno.i_s = newContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < newContent.length()) {
        int blockSize = std::min(64, (int)(newContent.length() - contentOffset));
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, newContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Usuario " + userName + " eliminado exitosamente.";
}

std::string CommandHandler::cmdChgrp(const std::map<std::string, std::string>& params) {
    // Verificar sesión
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Verificar que es root
    if (currentUser != "root") {
        return "Error: Solo root puede cambiar grupos de usuarios.";
    }
    
    std::vector<std::string> mandatory = {"user", "grp"};
    std::string validation = validateMandatoryParams(params, mandatory);
    if (!validation.empty()) return validation;
    
    std::string userName = CommandParser::getParameter(params, "user");
    std::string newGroupName = CommandParser::getParameter(params, "grp");
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición
    Partition* part = nullptr;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == currentPartitionId) {
            part = &mbr.mbr_partitions[i];
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Buscar users.txt en el directorio raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el directorio raíz.";
    }
    
    int usersInodeNum = -1;
    for (int i = 0; i < 4; i++) {
        if (std::string(rootBlock.b_content[i].b_name) == "users.txt") {
            usersInodeNum = rootBlock.b_content[i].b_inodo;
            break;
        }
    }
    
    if (usersInodeNum < 0) {
        return "Error: Archivo users.txt no encontrado.";
    }
    
    // Leer inodo de users.txt
    Inodo usersIno;
    if (!DiskManager::readInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo leer el inodo de users.txt.";
    }
    
    // Leer contenido de users.txt
    std::string usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                   (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Parsear y validar
    std::istringstream iss(usersContent);
    std::string line;
    bool userFound = false;
    bool groupExists = false;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        if (fields.size() >= 3) {
            // Verificar si el usuario existe y está activo
            if (fields[1] == "U" && fields.size() >= 4 && fields[3] == userName) {
                try {
                    int id = std::stoi(fields[0]);
                    if (id > 0) {  // Activo
                        userFound = true;
                    }
                } catch (...) {}
            }
            
            // Verificar si el nuevo grupo existe y está activo
            if (fields[1] == "G" && fields.size() >= 3 && fields[2] == newGroupName) {
                try {
                    int id = std::stoi(fields[0]);
                    if (id > 0) {  // Activo
                        groupExists = true;
                    }
                } catch (...) {}
            }
        }
    }
    
    if (!userFound) {
        return "Error: Usuario no encontrado o está eliminado: " + userName;
    }
    
    if (!groupExists) {
        return "Error: Grupo no existe o está eliminado: " + newGroupName;
    }
    
    // Ahora actualizar el grupo del usuario
    iss.clear();
    iss.seekg(0);
    usersContent = "";
    iss.str(usersContent);  // Reset stream
    
    // Releer users.txt para actualizar
    usersContent = "";
    for (int i = 0; i < 12 && usersIno.i_block[i] >= 0; i++) {
        BlockFile block;
        int blockSize = std::min(64, usersIno.i_s - (i * 64));
        if (!DiskManager::readBlock(diskPath, part->part_start, usersIno.i_block[i], 
                                   (char*)&block, blockSize)) {
            return "Error: No se pudo leer bloques de users.txt.";
        }
        usersContent.append(block.b_content, blockSize);
    }
    
    // Procesar línea por línea y actualizar el grupo del usuario
    std::istringstream issUpdate(usersContent);
    std::string newContent = "";
    
    while (std::getline(issUpdate, line)) {
        if (line.empty()) {
            newContent += "\n";
            continue;
        }
        
        std::istringstream lineStream(line);
        std::string field;
        std::vector<std::string> fields;
        
        while (std::getline(lineStream, field, ',')) {
            // Trim espacios
            size_t start = field.find_first_not_of(" ");
            size_t end = field.find_last_not_of(" ");
            if (start != std::string::npos) {
                field = field.substr(start, end - start + 1);
            }
            fields.push_back(field);
        }
        
        // Si es el usuario que queremos actualizar
        if (fields.size() >= 5 && fields[1] == "U" && fields[3] == userName) {
            // Formato: ID, U, NuevoGrupo, Usuario, Contraseña
            newContent += fields[0] + ", U, " + newGroupName + ", " + fields[3] + ", " + fields[4] + "\n";
        } else {
            newContent += line + "\n";
        }
    }
    
    // Remover última línea en blanco si existe
    if (!newContent.empty() && newContent.back() == '\n') {
        newContent.pop_back();
    }
    
    // Actualizar tamaño del inodo
    usersIno.i_s = newContent.length();
    
    // Escribir los bloques con el nuevo contenido
    int blockIndex = 0;
    size_t contentOffset = 0;
    
    while (contentOffset < newContent.length()) {
        int blockSize = std::min(64, (int)(newContent.length() - contentOffset));
        
        BlockFile block;
        memset(&block, 0, sizeof(BlockFile));
        std::memcpy(block.b_content, newContent.c_str() + contentOffset, blockSize);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, usersIno.i_block[blockIndex], 
                                    (char*)&block, sizeof(BlockFile))) {
            return "Error: No se pudo escribir bloque de users.txt.";
        }
        
        contentOffset += blockSize;
        blockIndex++;
    }
    
    // Actualizar timestamp del inodo
    usersIno.i_mtime = time(nullptr);
    
    // Escribir inodo actualizado
    if (!DiskManager::writeInodo(diskPath, part->part_start, usersInodeNum, usersIno)) {
        return "Error: No se pudo actualizar el inodo de users.txt.";
    }
    
    return "Usuario " + userName + " asignado al grupo " + newGroupName + " exitosamente.";
}

std::string CommandHandler::cmdMkdir(const std::map<std::string, std::string>& params) {
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Validar parámetro -path obligatorio
    std::string dirPath = CommandParser::getParameter(params, "path");
    if (dirPath.empty()) {
        return "Error: Parámetro obligatorio faltante: -path";
    }
    
    // Validar parámetro -p (no debe tener valor)
    std::string pParam = CommandParser::getParameter(params, "p", "");
    bool createParents = !pParam.empty();
    
    // Parsear ruta para obtener nombre de carpeta
    std::string folderName;
    std::string parentPath = "";
    
    size_t lastSlash = dirPath.find_last_of("/");
    if (lastSlash != std::string::npos && lastSlash > 0) {
        parentPath = dirPath.substr(0, lastSlash);
        folderName = dirPath.substr(lastSlash + 1);
    } else if (lastSlash == std::string::npos) {
        folderName = dirPath;
        parentPath = "/";
    } else {
        folderName = dirPath;
        parentPath = "/";
    }
    
    // Si parentPath es vacío, es raíz
    if (parentPath.empty()) {
        parentPath = "/";
    }
    
    // Validar que carpetas padres existan
    if (parentPath != "/" && !createParents) {
        return "Error: Las carpetas padres no existen. Use -p para crearlas.";
    }
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición
    Partition* part = nullptr;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == currentPartitionId) {
            part = &mbr.mbr_partitions[i];
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Leer bloque raíz (solo soportamos raíz en proyecto 1)
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        return "Error: No se pudo leer el bloque raíz.";
    }
    
    // Verificar si la carpeta ya existe
    for (int i = 0; i < 4; i++) {
        if (rootBlock.b_content[i].b_inodo != 0 && 
            std::string(rootBlock.b_content[i].b_name) == folderName) {
            return "Error: La carpeta ya existe: " + folderName;
        }
    }
    
    // Asignar nuevo inodo
    int newInode = DiskManager::allocateInode(diskPath, part->part_start, sb);
    if (newInode < 0) {
        return "Error: No hay inodos disponibles.";
    }
    
    // Asignar bloque al directorio
    int dirBlock = DiskManager::allocateBlock(diskPath, part->part_start, sb);
    if (dirBlock < 0) {
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No hay bloques disponibles.";
    }
    
    // Obtener UID/GID del usuario actual
    int userUid = (currentUser == "root") ? 1 : 2;
    int userGid = 1;  // Grupo por defecto
    
    // Crear inodo del directorio
    Inodo newDirIno;
    newDirIno.i_uid = userUid;
    newDirIno.i_gid = userGid;
    newDirIno.i_s = 0;
    newDirIno.i_atime = time(nullptr);
    newDirIno.i_ctime = time(nullptr);
    newDirIno.i_mtime = time(nullptr);
    newDirIno.i_type = 0;  
    std::strncpy(newDirIno.i_perm, "664", 3); 
    
    for (int i = 0; i < 15; i++) {
        newDirIno.i_block[i] = (i == 0) ? dirBlock : -1;
    }
    
    // Inicializar bloque del directorio 
    BlockFolder newDirBlock;
    memset(&newDirBlock, 0, sizeof(BlockFolder));
    
    // Primer registro: "." -> apunta al inodo del directorio actual
    std::strncpy(newDirBlock.b_content[0].b_name, ".", 11);
    newDirBlock.b_content[0].b_inodo = newInode;
    
    // Segundo registro: ".." -> apunta al inodo padre (raíz)
    std::strncpy(newDirBlock.b_content[1].b_name, "..", 11);
    newDirBlock.b_content[1].b_inodo = 0;
    
    // Los otros registros ya están en 0 por memset
    
    // Escribir bloque del directorio
    if (!DiskManager::writeBlock(diskPath, part->part_start, dirBlock, (char*)&newDirBlock, sizeof(BlockFolder))) {
        DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo escribir el bloque del directorio.";
    }
    
    // Escribir inodo
    if (!DiskManager::writeInodo(diskPath, part->part_start, newInode, newDirIno)) {
        DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo escribir el inodo.";
    }
    
    // Leer bloque raíz de nuevo (puede haber cambiado)
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo leer el bloque raíz.";
    }
    
    // Agregar entrada en raíz
    bool added = false;
    for (int i = 0; i < 4; i++) {
        if (rootBlock.b_content[i].b_inodo == 0) {
            std::strncpy(rootBlock.b_content[i].b_name, folderName.c_str(), 11);
            rootBlock.b_content[i].b_inodo = newInode;
            added = true;
            break;
        }
    }
    
    if (!added) {
        DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No hay espacio en el directorio raíz.";
    }
    
    // Escribir bloque raíz actualizado
    if (!DiskManager::writeBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        DiskManager::deallocateBlock(diskPath, part->part_start, dirBlock, sb);
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo escribir el bloque raíz.";
    }
    
    // Escribir superblock actualizado
    if (!DiskManager::writeSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo actualizar el superblock.";
    }
    
    return "Directorio " + folderName + " creado exitosamente.";
}

std::string CommandHandler::cmdMkfile(const std::map<std::string, std::string>& params) {
    if (!isLoggedIn) {
        return "Error: Debe iniciar sesión.";
    }
    
    // Validar parámetro -path obligatorio
    std::string filePath = CommandParser::getParameter(params, "path");
    if (filePath.empty()) {
        return "Error: Parámetro obligatorio faltante: -path";
    }
    
    // Validar parámetro -r (no debe tener valor)
    std::string rParam = CommandParser::getParameter(params, "r", "");
    bool createDirs = !rParam.empty();
    
    // Validar parámetro -size
    std::string sizeStr = CommandParser::getParameter(params, "size", "0");
    int fileSize = 0;
    try {
        fileSize = std::stoi(sizeStr);
    } catch (...) {
        return "Error: Parámetro -size inválido.";
    }
    
    if (fileSize < 0) {
        return "Error: El tamaño del archivo no puede ser negativo.";
    }
    
    // Validar parámetro -cont
    std::string contPath = CommandParser::getParameter(params, "cont", "");
    std::string fileContent = "";
    
    if (!contPath.empty()) {
        // Intentar leer archivo del sistema
        std::ifstream contFile(contPath, std::ios::binary);
        if (!contFile) {
            return "Error: No se pudo leer el archivo: " + contPath;
        }
        
        // Leer todo el contenido
        contFile.seekg(0, std::ios::end);
        size_t contSize = contFile.tellg();
        contFile.seekg(0, std::ios::beg);
        
        fileContent.resize(contSize);
        contFile.read(&fileContent[0], contSize);
        contFile.close();
        
        fileSize = contSize;  
    }
    
    // Si no hay contenido externo, generar contenido con patrón 0-9
    if (fileContent.empty() && fileSize > 0) {
        for (int i = 0; i < fileSize; i++) {
            fileContent += char('0' + (i % 10));
        }
    }
    
    // Obtener ruta del disco
    if (mountedPartitions.find(currentPartitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada.";
    }
    
    std::string diskPath = mountedPartitions[currentPartitionId];
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición
    Partition* part = nullptr;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == currentPartitionId) {
            part = &mbr.mbr_partitions[i];
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Parsear ruta para obtener padre y nombre
    std::string fileName;
    std::string dirPath = "";
    
    size_t lastSlash = filePath.find_last_of("/");
    if (lastSlash != std::string::npos) {
        dirPath = filePath.substr(0, lastSlash);
        fileName = filePath.substr(lastSlash + 1);
    } else {
        fileName = filePath;
        dirPath = "/";
    }
    
    // Si dirPath es vacío, es raíz
    if (dirPath.empty()) {
        dirPath = "/";
    }
    
    // Para simplificar: solo soportamos archivos en raíz por ahora
    if (dirPath != "/" && !createDirs) {
        return "Error: Las carpetas padres no existen. Use -r para crearlas.";
    }
    
    int parentInode = 0;  
    // Asignar nuevo inodo para el archivo
    int newInode = DiskManager::allocateInode(diskPath, part->part_start, sb);
    if (newInode < 0) {
        return "Error: No hay inodos disponibles.";
    }
    
    // Calcular bloques necesarios (bloques de 64 bytes)
    int blocksNeeded = (fileSize + 63) / 64;
    if (blocksNeeded > 12) {
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: Archivo muy grande (máximo 768 bytes).";
    }
    
    // Asignar bloques
    std::vector<int> fileBlocks;
    for (int i = 0; i < blocksNeeded; i++) {
        int block = DiskManager::allocateBlock(diskPath, part->part_start, sb);
        if (block < 0) {
            for (int b : fileBlocks) {
                DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
            }
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No hay bloques disponibles.";
        }
        fileBlocks.push_back(block);
    }
    
    // Obtener UID/GID del usuario actual (por simplificar, root es 1, otros son 2+)
    int userUid = (currentUser == "root") ? 1 : 2;
    int userGid = 1;  
    
    // Crear inodo del archivo
    Inodo fileIno;
    fileIno.i_uid = userUid;
    fileIno.i_gid = userGid;
    fileIno.i_s = fileSize;
    fileIno.i_atime = time(nullptr);
    fileIno.i_ctime = time(nullptr);
    fileIno.i_mtime = time(nullptr);
    fileIno.i_type = 1;  // 1 = archivo
    std::strncpy(fileIno.i_perm, "664", 3);  // rw-rw-r--
    
    for (int i = 0; i < 15; i++) {
        fileIno.i_block[i] = (i < blocksNeeded) ? fileBlocks[i] : -1;
    }
    
    // Escribir bloques de archivo
    int bytesWritten = 0;
    for (int i = 0; i < blocksNeeded; i++) {
        BlockFile fileBlock;
        memset(&fileBlock, 0, sizeof(BlockFile));
        
        int bytesToWrite = std::min(64, fileSize - bytesWritten);
        std::memcpy(fileBlock.b_content, fileContent.c_str() + bytesWritten, bytesToWrite);
        
        if (!DiskManager::writeBlock(diskPath, part->part_start, fileBlocks[i], (char*)&fileBlock, sizeof(BlockFile))) {
            for (int b : fileBlocks) {
                DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
            }
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: No se pudo escribir bloque del archivo.";
        }
        
        bytesWritten += bytesToWrite;
    }
    
    // Escribir inodo del archivo
    if (!DiskManager::writeInodo(diskPath, part->part_start, newInode, fileIno)) {
        for (int b : fileBlocks) {
            DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
        }
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo escribir el inodo.";
    }
    
    // Leer bloque raíz
    BlockFolder rootBlock;
    if (!DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        for (int b : fileBlocks) {
            DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
        }
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo leer el bloque raíz.";
    }
    
    // Verificar si el archivo ya existe
    for (int i = 0; i < 4; i++) {
        if (rootBlock.b_content[i].b_inodo != 0 && 
            std::string(rootBlock.b_content[i].b_name) == fileName) {
            // Archivo existe - para simplificar, retornamos error en lugar de preguntar
            for (int b : fileBlocks) {
                DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
            }
            DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
            return "Error: El archivo ya existe: " + fileName;
        }
    }
    
    // Agregar entrada en raíz
    bool added = false;
    for (int i = 0; i < 4; i++) {
        if (rootBlock.b_content[i].b_inodo == 0) {
            std::strncpy(rootBlock.b_content[i].b_name, fileName.c_str(), 11);
            rootBlock.b_content[i].b_inodo = newInode;
            added = true;
            break;
        }
    }
    
    if (!added) {
        for (int b : fileBlocks) {
            DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
        }
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No hay espacio en el directorio raíz.";
    }
    
    // Escribir bloque raíz actualizado
    if (!DiskManager::writeBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
        for (int b : fileBlocks) {
            DiskManager::deallocateBlock(diskPath, part->part_start, b, sb);
        }
        DiskManager::deallocateInode(diskPath, part->part_start, newInode, sb);
        return "Error: No se pudo escribir el bloque raíz.";
    }
    
    // Escribir superblock actualizado
    if (!DiskManager::writeSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo actualizar el superblock.";
    }
    
    return "Archivo " + fileName + " creado exitosamente.";
}

std::string CommandHandler::cmdRep(const std::map<std::string, std::string>& params) {
    // Validar parámetros obligatorios
    std::string repName = CommandParser::getParameter(params, "name");
    if (repName.empty()) {
        return "Error: Parámetro obligatorio faltante: -name";
    }
    
    std::string repPath = CommandParser::getParameter(params, "path");
    if (repPath.empty()) {
        return "Error: Parámetro obligatorio faltante: -path";
    }
    
    std::string partitionId = CommandParser::getParameter(params, "id");
    if (partitionId.empty()) {
        return "Error: Parámetro obligatorio faltante: -id";
    }
    
    // Validar tipo de reporte
    std::vector<std::string> validReports = {"mbr", "disk", "inode", "block", "bm_inode", "bm_bloc", "tree", "sb", "file", "ls", "ebr"};
    if (std::find(validReports.begin(), validReports.end(), repName) == validReports.end()) {
        return "Error: Tipo de reporte inválido: " + repName;
    }
    
    // Validar que la partición existe
    if (mountedPartitions.find(partitionId) == mountedPartitions.end()) {
        return "Error: Partición no montada con ID: " + partitionId;
    }
    
    std::string diskPath = mountedPartitions[partitionId];
    
    // Crear carpeta si no existe
    std::string folderPath = repPath;
    size_t lastSlash = repPath.find_last_of("/");
    if (lastSlash != std::string::npos) {
        folderPath = repPath.substr(0, lastSlash);
        if (!folderPath.empty()) {
            system(("mkdir -p \"" + folderPath + "\" 2>/dev/null").c_str());
        }
    }
    
    // Leer MBR
    MBR mbr;
    if (!DiskManager::readMBR(diskPath, mbr)) {
        return "Error: No se pudo leer el MBR.";
    }
    
    // Encontrar partición
    Partition* part = nullptr;
    for (int i = 0; i < 4; i++) {
        if (std::string(mbr.mbr_partitions[i].part_id) == partitionId) {
            part = &mbr.mbr_partitions[i];
            break;
        }
    }
    
    if (part == nullptr) {
        return "Error: Partición no encontrada.";
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, part->part_start, sb)) {
        return "Error: No se pudo leer el superblock.";
    }
    
    // Generar reporte según tipo
    std::string graphvizContent = "";
    
    if (repName == "mbr") {
        // Reporte de MBR - Detallado
        graphvizContent = "digraph MBR {\n";
        graphvizContent += "  rankdir=TB;\n";
        graphvizContent += "  node [shape=plaintext];\n";
        graphvizContent += "  MBR [label=<\n";
        graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\">\n";
        
        // Encabezado del MBR
        graphvizContent += "      <TR><TD COLSPAN=\"8\" BGCOLOR=\"#800080\"><B><FONT COLOR=\"white\">REPORTE DE MBR</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD>mbr_tamano</TD><TD>" + std::to_string(mbr.mbr_tamano) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>mbr_fecha_creacion</TD><TD>" + std::string(__DATE__) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>mbr_dsk_signature</TD><TD>" + std::to_string(mbr.mbr_dsk_signature) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD COLSPAN=\"8\"></TD></TR>\n";
        
        // Particiones Primarias
        graphvizContent += "      <TR><TD COLSPAN=\"8\" BGCOLOR=\"#A020F0\"><B><FONT COLOR=\"white\">Particiones Primarias</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD><B>part_status</B></TD><TD><B>part_type</B></TD><TD><B>part_fit</B></TD><TD><B>part_start</B></TD><TD><B>part_s</B></TD><TD><B>part_name</B></TD><TD><B>part_correlative</B></TD><TD><B>part_id</B></TD></TR>\n";
        
        bool hasPrimary = false;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'P') {
                hasPrimary = true;
                graphvizContent += "      <TR>";
                graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_status) + "</TD>";
                graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_type) + "</TD>";
                graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_fit) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_start) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_s) + "</TD>";
                graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_name) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_correlative) + "</TD>";
                graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_id) + "</TD>";
                graphvizContent += "</TR>\n";
            }
        }
        
        if (!hasPrimary) {
            graphvizContent += "      <TR><TD COLSPAN=\"8\">-</TD></TR>\n";
        }
        
        // Particiones Extendidas
        graphvizContent += "      <TR><TD COLSPAN=\"8\" BGCOLOR=\"#FF6347\"><B><FONT COLOR=\"white\">Particiones Extendidas</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD><B>part_status</B></TD><TD><B>part_type</B></TD><TD><B>part_fit</B></TD><TD><B>part_start</B></TD><TD><B>part_s</B></TD><TD><B>part_name</B></TD><TD><B>part_correlative</B></TD><TD><B>part_id</B></TD></TR>\n";
        
        bool hasExtended = false;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'E') {
                hasExtended = true;
                graphvizContent += "      <TR>";
                graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_status) + "</TD>";
                graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_type) + "</TD>";
                graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_fit) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_start) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_s) + "</TD>";
                graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_name) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_correlative) + "</TD>";
                graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_id) + "</TD>";
                graphvizContent += "</TR>\n";
            }
        }
        
        if (!hasExtended) {
            graphvizContent += "      <TR><TD COLSPAN=\"8\">-</TD></TR>\n";
        }
        
        // Particiones Lógicas (simplificado - estarían en EBR en proyecto 2)
        graphvizContent += "      <TR><TD COLSPAN=\"8\" BGCOLOR=\"#4169E1\"><B><FONT COLOR=\"white\">Particiones Lógicas</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD><B>part_status</B></TD><TD><B>part_type</B></TD><TD><B>part_fit</B></TD><TD><B>part_start</B></TD><TD><B>part_s</B></TD><TD><B>part_name</B></TD><TD><B>part_correlative</B></TD><TD><B>part_id</B></TD></TR>\n";
        
        bool hasLogical = false;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'L') {
                hasLogical = true;
                graphvizContent += "      <TR>";
                graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_status) + "</TD>";
                graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_type) + "</TD>";
                graphvizContent += "<TD>" + std::string(1, mbr.mbr_partitions[i].part_fit) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_start) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_s) + "</TD>";
                graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_name) + "</TD>";
                graphvizContent += "<TD>" + std::to_string(mbr.mbr_partitions[i].part_correlative) + "</TD>";
                graphvizContent += "<TD>" + std::string(mbr.mbr_partitions[i].part_id) + "</TD>";
                graphvizContent += "</TR>\n";
            }
        }
        
        if (!hasLogical) {
            graphvizContent += "      <TR><TD COLSPAN=\"8\">-</TD></TR>\n";
        }
        
        graphvizContent += "    </TABLE>\n";
        graphvizContent += "  >];\n";
        graphvizContent += "}\n";
        
    } else if (repName == "sb") {
        // Reporte de Superblock - Información Completa
        graphvizContent = "digraph Superblock {\n";
        graphvizContent += "  rankdir=TB;\n";
        graphvizContent += "  node [shape=plaintext];\n";
        graphvizContent += "  SB [label=<\n";
        graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\">\n";
        
        // Encabezado
        graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Reporte de SUPERBLOQUE</FONT></B></TD></TR>\n";
        
        // Fila de separador
        graphvizContent += "      <TR><TD COLSPAN=\"2\"></TD></TR>\n";
        
        // Información General del Filesystem
        graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Información General</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD>s_filesystem_type</TD><TD>" + std::to_string(sb.s_filesystem_type) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_inodes_count</TD><TD>" + std::to_string(sb.s_inodes_count) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>s_blocks_count</TD><TD>" + std::to_string(sb.s_blocks_count) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_free_inodes_count</TD><TD>" + std::to_string(sb.s_free_inodes_count) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>s_free_blocks_count</TD><TD>" + std::to_string(sb.s_free_blocks_count) + "</TD></TR>\n";
        
        // Información de Fechas y Montajes
        graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Fechas y Montajes</FONT></B></TD></TR>\n";
        std::string mtimeStr = std::string(ctime(&sb.s_mtime));
        std::string umtimeStr = std::string(ctime(&sb.s_umtime));
        if (!mtimeStr.empty() && mtimeStr.back() == '\n') mtimeStr.pop_back();
        if (!umtimeStr.empty() && umtimeStr.back() == '\n') umtimeStr.pop_back();
        graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_mtime (fecha creación)</TD><TD>" + mtimeStr + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>s_umtime (último montaje)</TD><TD>" + umtimeStr + "</TD></TR>\n";
        graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_mnt_count</TD><TD>" + std::to_string(sb.s_mnt_count) + "</TD></TR>\n";
        
        // Información de Tamaños de Estructuras
        graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Tamaños de Estructuras</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD>s_inode_s (tamaño inodo)</TD><TD>" + std::to_string(sb.s_inode_s) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_block_s (tamaño bloque)</TD><TD>" + std::to_string(sb.s_block_s) + "</TD></TR>\n";
        
        // Información de Primeros Inodos/Bloques
        graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Primeros Inodos/Bloques</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD>s_firts_ino</TD><TD>" + std::to_string(sb.s_firts_ino) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_first_blo</TD><TD>" + std::to_string(sb.s_first_blo) + "</TD></TR>\n";
        
        // Información de Posiciones en Disco
        graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Posiciones en Disco</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD>s_bm_inode_start</TD><TD>" + std::to_string(sb.s_bm_inode_start) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_bm_block_start</TD><TD>" + std::to_string(sb.s_bm_block_start) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>s_inode_start</TD><TD>" + std::to_string(sb.s_inode_start) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD BGCOLOR=\"#90EE90\">s_block_start</TD><TD>" + std::to_string(sb.s_block_start) + "</TD></TR>\n";
        
        // Información de Identificación
        graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Identificación</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD>s_magic</TD><TD>0x" + std::to_string(sb.s_magic) + "</TD></TR>\n";
        
        graphvizContent += "    </TABLE>\n";
        graphvizContent += "  >];\n";
        graphvizContent += "}\n";
        
    } else if (repName == "inode") {
        // Reporte de Inodos - Mostrar cada inodo utilizado en su propio bloque
        graphvizContent = "digraph Inodes {\n";
        graphvizContent += "  rankdir=TB;\n";
        graphvizContent += "  node [shape=plaintext];\n";
        
        int usedInodeCount = 0;
        
        // Iterar sobre todos los inodos del sistema
        for (int i = 0; i < sb.s_inodes_count; i++) {
            Inodo ino;
            if (DiskManager::readInodo(diskPath, part->part_start, i, ino)) {
                // Solo mostrar si el inodo está siendo utilizado (tipo válido)
                if (ino.i_type >= 0) {
                    std::string type = (ino.i_type == 0) ? "Dir" : ((ino.i_type == 1) ? "File" : "Unknown");
                    
                    // Convertir timestamp a string legible
                    std::string atimeStr = std::string(ctime(&ino.i_atime));
                    // Remover el salto de línea al final que agrega ctime
                    if (!atimeStr.empty() && atimeStr.back() == '\n') {
                        atimeStr.pop_back();
                    }
                    
                    // Crear nodo para este inodo
                    std::string nodeId = "inode_" + std::to_string(i);
                    graphvizContent += "  " + nodeId + " [label=<\n";
                    graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" BGCOLOR=\"#F0F0F0\">\n";
                    graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#800080\"><B><FONT COLOR=\"white\">Inodo " + std::to_string(i) + "</FONT></B></TD></TR>\n";
                    graphvizContent += "      <TR><TD>i_uid</TD><TD>" + std::to_string(ino.i_uid) + "</TD></TR>\n";
                    graphvizContent += "      <TR><TD>i_size</TD><TD>" + std::to_string(ino.i_s) + "</TD></TR>\n";
                    graphvizContent += "      <TR><TD>i_atime</TD><TD>" + atimeStr + "</TD></TR>\n";
                    
                    // Mostrar los bloques asociados
                    bool hasBlocks = false;
                    for (int j = 0; j < 15; j++) {
                        if (ino.i_block[j] != -1) {
                            if (!hasBlocks) {
                                graphvizContent += "      <TR><TD>i_block_" + std::to_string(j+1) + "</TD><TD>" + std::to_string(ino.i_block[j]) + "</TD></TR>\n";
                                hasBlocks = true;
                            } else {
                                graphvizContent += "      <TR><TD>i_block_" + std::to_string(j+1) + "</TD><TD>" + std::to_string(ino.i_block[j]) + "</TD></TR>\n";
                            }
                        }
                    }
                    
                    if (!hasBlocks) {
                        graphvizContent += "      <TR><TD>i_block_1</TD><TD>-1</TD></TR>\n";
                    }
                    
                    graphvizContent += "      <TR><TD>i_perm</TD><TD>" + std::string(ino.i_perm) + "</TD></TR>\n";
                    graphvizContent += "    </TABLE>\n";
                    graphvizContent += "  >];\n";
                    
                    usedInodeCount++;
                }
            }
        }
        
        // Si no hay inodos utilizados, mostrar un mensaje
        if (usedInodeCount == 0) {
            graphvizContent += "  empty [label=\"No hay inodos utilizados\"];\n";
        }
        
        graphvizContent += "}\n";
        
    } else if (repName == "block") {
        // Reporte de todos los bloques utilizados (Carpetas y Archivos)
        graphvizContent = "digraph Blocks {\n";
        graphvizContent += "  rankdir=TB;\n";
        graphvizContent += "  node [shape=plaintext];\n";
        
        int blockCount = 0;
        
        // Iterar sobre todos los inodos para encontrar bloques utilizados
        for (int inodeIdx = 0; inodeIdx < sb.s_inodes_count; inodeIdx++) {
            Inodo ino;
            if (!DiskManager::readInodo(diskPath, part->part_start, inodeIdx, ino)) {
                continue;
            }
            
            // Solo procesar inodos utilizados
            if (ino.i_type < 0) {
                continue;
            }
            
            // Procesar cada bloque del inodo
            for (int blockIdx = 0; blockIdx < 15 && ino.i_block[blockIdx] != -1; blockIdx++) {
                int blockNum = ino.i_block[blockIdx];
                std::string nodeId = "block_" + std::to_string(blockNum);
                
                // Crear nodo para este bloque
                graphvizContent += "  " + nodeId + " [label=<\n";
                graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" BGCOLOR=\"#F0F0F0\">\n";
                
                if (ino.i_type == 0) {
                    // Es un directorio - mostrar BlockFolder
                    BlockFolder blockFolder;
                    if (DiskManager::readBlock(diskPath, part->part_start, blockNum, 
                                              (char*)&blockFolder, sizeof(BlockFolder))) {
                        
                        graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#4169E1\"><B><FONT COLOR=\"white\">Bloque Carpeta " + std::to_string(blockNum) + "</FONT></B></TD></TR>\n";
                        graphvizContent += "      <TR><TD><B>b_name</B></TD><TD><B>b_inodo</B></TD></TR>\n";
                        
                        // Mostrar entradas del directorio
                        for (int j = 0; j < 4; j++) {
                            if (blockFolder.b_content[j].b_inodo != 0) {
                                graphvizContent += "      <TR><TD>" + std::string(blockFolder.b_content[j].b_name) + "</TD>";
                                graphvizContent += "<TD>" + std::to_string(blockFolder.b_content[j].b_inodo) + "</TD></TR>\n";
                            }
                        }
                    }
                    
                } else if (ino.i_type == 1) {
                    // Es un archivo - mostrar BlockFile
                    BlockFile blockFile;
                    if (DiskManager::readBlock(diskPath, part->part_start, blockNum, 
                                              (char*)&blockFile, sizeof(BlockFile))) {
                        
                        graphvizContent += "      <TR><TD COLSPAN=\"1\" BGCOLOR=\"#FF6347\"><B><FONT COLOR=\"white\">Bloque Archivo " + std::to_string(blockNum) + "</FONT></B></TD></TR>\n";
                        
                        // Mostrar contenido del archivo (limitado a 64 caracteres por legibilidad)
                        std::string content(blockFile.b_content);
                        if (content.length() > 64) {
                            content = content.substr(0, 64) + "...";
                        }
                        
                        // Escapar caracteres especiales para XML
                        std::string escapedContent = "";
                        for (char c : content) {
                            if (c == '<') escapedContent += "&lt;";
                            else if (c == '>') escapedContent += "&gt;";
                            else if (c == '&') escapedContent += "&amp;";
                            else if (c == '"') escapedContent += "&quot;";
                            else if (c == '\'') escapedContent += "&apos;";
                            else if (c == '\n') escapedContent += "\\n";
                            else if (c == '\t') escapedContent += "\\t";
                            else if (c >= 32 && c < 127) escapedContent += c;
                            else escapedContent += "?";
                        }
                        
                        graphvizContent += "      <TR><TD><FONT FACE=\"Courier\" SIZE=\"9\">" + escapedContent + "</FONT></TD></TR>\n";
                    }
                }
                
                graphvizContent += "    </TABLE>\n";
                graphvizContent += "  >];\n";
                blockCount++;
            }
        }
        
        // Si no hay bloques utilizados, mostrar mensaje
        if (blockCount == 0) {
            graphvizContent += "  empty [label=\"No hay bloques utilizados\"];\n";
        }
        
        graphvizContent += "}\n";
        
    } else if (repName == "bm_inode") {
        // Reporte de Mapa de Bits de Inodos - Archivo de texto con 20 bits por línea
        std::string textContent = "";
        
        // Calcular cantidad de bytes en el bitmap de inodos
        int bitmapBytes = (sb.s_inodes_count + 7) / 8;  // Redondear hacia arriba
        
        // Leer el bitmap desde el disco
        char* bitmapData = new char[bitmapBytes];
        if (!DiskManager::readFromDisk(diskPath, sb.s_bm_inode_start, bitmapData, bitmapBytes)) {
            delete[] bitmapData;
            return "Error: No se pudo leer el bitmap de inodos.";
        }
        
        // Generar contenido del reporte con 20 bits por línea
        int lineNumber = 1;
        int bitsPerLine = 20;
        int bitCount = 0;
        
        for (int byteIdx = 0; byteIdx < bitmapBytes; byteIdx++) {
            unsigned char currentByte = (unsigned char)bitmapData[byteIdx];
            
            // Procesar cada bit del byte
            for (int bitIdx = 0; bitIdx < 8 && bitCount < sb.s_inodes_count; bitIdx++) {
                // Extraer el bit (LSB primero)
                int bit = (currentByte >> bitIdx) & 1;
                
                // Si es el primer bit de la línea, agregar número de línea
                if (bitCount % bitsPerLine == 0) {
                    if (bitCount > 0) {
                        textContent += "\n";
                    }
                    textContent += std::to_string(lineNumber) + " ";
                    lineNumber++;
                }
                
                // Agregar el bit
                textContent += std::to_string(bit);
                
                // Agregar espacio entre bits (excepto el último)
                if ((bitCount + 1) % bitsPerLine != 0 && bitCount + 1 < sb.s_inodes_count) {
                    textContent += " ";
                }
                
                bitCount++;
            }
        }
        
        delete[] bitmapData;
        
        // Agregar salto de línea al final si es necesario
        if (!textContent.empty() && textContent.back() != '\n') {
            textContent += "\n";
        }
        
        // Guardar en archivo de texto
        std::string textFilePath = repPath;
        
        std::ofstream textFile(textFilePath);
        if (!textFile.is_open()) {
            return "Error: No se pudo crear el archivo de reporte: " + textFilePath;
        }
        
        textFile << textContent;
        textFile.close();
        
        return "Reporte bm_inode generado en: " + textFilePath;
        
    } else if (repName == "bm_bloc") {
        // Reporte de Mapa de Bits de Bloques - Archivo de texto con 20 bits por línea
        std::string textContent = "";
        
        // Calcular cantidad de bytes en el bitmap de bloques
        int bitmapBytes = (sb.s_blocks_count + 7) / 8;  // Redondear hacia arriba
        
        // Leer el bitmap desde el disco
        char* bitmapData = new char[bitmapBytes];
        if (!DiskManager::readFromDisk(diskPath, sb.s_bm_block_start, bitmapData, bitmapBytes)) {
            delete[] bitmapData;
            return "Error: No se pudo leer el bitmap de bloques.";
        }
        
        // Generar contenido del reporte con 20 bits por línea
        int lineNumber = 1;
        int bitsPerLine = 20;
        int bitCount = 0;
        
        for (int byteIdx = 0; byteIdx < bitmapBytes; byteIdx++) {
            unsigned char currentByte = (unsigned char)bitmapData[byteIdx];
            
            // Procesar cada bit del byte
            for (int bitIdx = 0; bitIdx < 8 && bitCount < sb.s_blocks_count; bitIdx++) {
                // Extraer el bit (LSB primero)
                int bit = (currentByte >> bitIdx) & 1;
                
                // Si es el primer bit de la línea, agregar número de línea
                if (bitCount % bitsPerLine == 0) {
                    if (bitCount > 0) {
                        textContent += "\n";
                    }
                    textContent += std::to_string(lineNumber) + " ";
                    lineNumber++;
                }
                
                // Agregar el bit
                textContent += std::to_string(bit);
                
                // Agregar espacio entre bits (excepto el último)
                if ((bitCount + 1) % bitsPerLine != 0 && bitCount + 1 < sb.s_blocks_count) {
                    textContent += " ";
                }
                
                bitCount++;
            }
        }
        
        delete[] bitmapData;
        
        // Agregar salto de línea al final si es necesario
        if (!textContent.empty() && textContent.back() != '\n') {
            textContent += "\n";
        }
        
        // Guardar en archivo de texto
        std::string textFilePath = repPath;
        
        std::ofstream textFile(textFilePath);
        if (!textFile.is_open()) {
            return "Error: No se pudo crear el archivo de reporte: " + textFilePath;
        }
        
        textFile << textContent;
        textFile.close();
        
        return "Reporte bm_bloc generado en: " + textFilePath;
        
    } else if (repName == "tree") {
        // Reporte de Árbol Completo del Sistema EXT2
        graphvizContent = "digraph FileSystemTree {\n";
        graphvizContent += "  rankdir=TB;\n";
        graphvizContent += "  node [shape=plaintext];\n";
        graphvizContent += "  edge [fontsize=10];\n";
        
        // Estructura para rastrear nodos y evitar ciclos
        std::set<int> visitedInodes;
        std::string edgesContent = "";
        
        // Función lambda para procesar inodos recursivamente
        std::function<void(int, int, const std::string&)> processInode = 
        [&](int inodeNum, int parentNum, const std::string& parentName) {
            if (visitedInodes.count(inodeNum) > 0) {
                return;  // Evitar ciclos
            }
            visitedInodes.insert(inodeNum);
            
            Inodo ino;
            if (!DiskManager::readInodo(diskPath, part->part_start, inodeNum, ino)) {
                return;
            }
            
            if (ino.i_type < 0) {
                return;  // Inodo no utilizado
            }
            
            std::string nodeId = "inode_" + std::to_string(inodeNum);
            
            // Convertir timestamp
            std::string atimeStr = std::string(ctime(&ino.i_atime));
            if (!atimeStr.empty() && atimeStr.back() == '\n') {
                atimeStr.pop_back();
            }
            
            // Crear nodo con información del inodo
            graphvizContent += "  " + nodeId + " [label=<\n";
            graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" BGCOLOR=\"#F0F0F0\">\n";
            
            if (ino.i_type == 0) {
                // Es un directorio
                graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#4169E1\"><B><FONT COLOR=\"white\">Dir: Inode " + std::to_string(inodeNum) + "</FONT></B></TD></TR>\n";
            } else {
                // Es un archivo
                graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#FF6347\"><B><FONT COLOR=\"white\">File: Inode " + std::to_string(inodeNum) + "</FONT></B></TD></TR>\n";
            }
            
            graphvizContent += "      <TR><TD>i_uid</TD><TD>" + std::to_string(ino.i_uid) + "</TD></TR>\n";
            graphvizContent += "      <TR><TD>i_size</TD><TD>" + std::to_string(ino.i_s) + "</TD></TR>\n";
            graphvizContent += "      <TR><TD>i_atime</TD><TD><FONT SIZE=\"8\">" + atimeStr + "</FONT></TD></TR>\n";
            
            // Mostrar bloques
            for (int i = 0; i < 15 && ino.i_block[i] != -1; i++) {
                graphvizContent += "      <TR><TD>i_block[" + std::to_string(i) + "]</TD><TD>" + std::to_string(ino.i_block[i]) + "</TD></TR>\n";
            }
            
            graphvizContent += "      <TR><TD>i_perm</TD><TD>" + std::string(ino.i_perm) + "</TD></TR>\n";
            graphvizContent += "    </TABLE>\n";
            graphvizContent += "  >];\n";
            
            // Conectar con padre si existe
            if (parentNum >= 0) {
                std::string parentNodeId = "inode_" + std::to_string(parentNum);
                edgesContent += "  " + parentNodeId + " -> " + nodeId + ";\n";
            }
            
            // Si es directorio, procesar entradas
            if (ino.i_type == 0) {
                for (int blockIdx = 0; blockIdx < 15 && ino.i_block[blockIdx] != -1; blockIdx++) {
                    BlockFolder dirBlock;
                    if (DiskManager::readBlock(diskPath, part->part_start, ino.i_block[blockIdx], 
                                              (char*)&dirBlock, sizeof(BlockFolder))) {
                        
                        for (int entryIdx = 0; entryIdx < 4; entryIdx++) {
                            if (dirBlock.b_content[entryIdx].b_inodo > 0) {
                                // Recursivamente procesar el inodo hijo
                                processInode(dirBlock.b_content[entryIdx].b_inodo, inodeNum, 
                                            std::string(dirBlock.b_content[entryIdx].b_name));
                            }
                        }
                    }
                }
            }
        };
        
        // Iniciar desde el inodo raíz (generalmente inodo 1 o 0)
        processInode(0, -1, "/");
        
        // Agregar aristas
        graphvizContent += edgesContent;
        graphvizContent += "}\n";
        
    } else if (repName == "file") {
        // Reporte de Archivo - Nombre y contenido completo
        std::string filePathParam = CommandParser::getParameter(params, "path_file_ls");
        if (filePathParam.empty()) {
            return "Error: Parámetro -path_file_ls requerido para reporte file";
        }
        
        // Buscar archivo en raíz
        BlockFolder rootBlock;
        int targetInode = -1;
        std::string targetFileName = "";
        
        if (DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
            for (int i = 0; i < 4; i++) {
                if (std::string(rootBlock.b_content[i].b_name) == filePathParam) {
                    targetInode = rootBlock.b_content[i].b_inodo;
                    targetFileName = std::string(rootBlock.b_content[i].b_name);
                    break;
                }
            }
        }
        
        if (targetInode < 0) {
            return "Error: Archivo no encontrado: " + filePathParam;
        }
        
        Inodo targetIno;
        if (!DiskManager::readInodo(diskPath, part->part_start, targetInode, targetIno)) {
            return "Error: No se pudo leer el inodo del archivo.";
        }
        
        // El reporte file solo funciona con archivos, no directorios
        if (targetIno.i_type != 1) {
            return "Error: La ruta especificada no es un archivo.";
        }
        
        // Leer todo el contenido del archivo desde sus bloques
        std::string fileContent = "";
        int bytesRead = 0;
        
        for (int blockIdx = 0; blockIdx < 15 && targetIno.i_block[blockIdx] != -1; blockIdx++) {
            BlockFile blockFile;
            if (DiskManager::readBlock(diskPath, part->part_start, targetIno.i_block[blockIdx], 
                                      (char*)&blockFile, sizeof(BlockFile))) {
                
                // Leer solo los bytes que realmente pertenecen al archivo
                int bytesToRead = std::min(64, (int)(targetIno.i_s - bytesRead));
                fileContent += std::string(blockFile.b_content, bytesToRead);
                bytesRead += bytesToRead;
                
                if (bytesRead >= targetIno.i_s) {
                    break;  // Ya leímos todo el contenido
                }
            }
        }
        
        // Crear contenido del reporte en texto
        std::string textContent = "=== REPORTE DE ARCHIVO ===\n\n";
        textContent += "Nombre del archivo: " + targetFileName + "\n";
        textContent += "Tamaño: " + std::to_string(targetIno.i_s) + " bytes\n";
        textContent += "Propietario (UID): " + std::to_string(targetIno.i_uid) + "\n";
        textContent += "Grupo (GID): " + std::to_string(targetIno.i_gid) + "\n";
        textContent += "Permisos: " + std::string(targetIno.i_perm) + "\n";
        textContent += "\n=== CONTENIDO DEL ARCHIVO ===\n\n";
        textContent += fileContent + "\n\n";
        textContent += "=== FIN DEL ARCHIVO ===\n";
        
        // Guardar en archivo de texto
        std::string textFilePath = repPath;
        
        std::ofstream textFile(textFilePath);
        if (!textFile.is_open()) {
            return "Error: No se pudo crear el archivo de reporte: " + textFilePath;
        }
        
        textFile << textContent;
        textFile.close();
        
        return "Reporte file generado en: " + textFilePath;
        
    } else if (repName == "ls") {
        // Reporte de Listado de Directorio - Información Detallada
        std::string filePathParam = CommandParser::getParameter(params, "path_file_ls");
        if (filePathParam.empty()) {
            return "Error: Parámetro -path_file_ls requerido para reporte ls";
        }
        
        // Buscar directorio en raíz (simplificado)
        BlockFolder rootBlock;
        int targetInode = -1;
        std::string targetDirName = "";
        
        if (DiskManager::readBlock(diskPath, part->part_start, 0, (char*)&rootBlock, sizeof(BlockFolder))) {
            for (int i = 0; i < 4; i++) {
                if (std::string(rootBlock.b_content[i].b_name) == filePathParam || filePathParam == "/") {
                    if (filePathParam == "/") {
                        targetInode = 0;
                        targetDirName = "/";
                    } else {
                        targetInode = rootBlock.b_content[i].b_inodo;
                        targetDirName = std::string(rootBlock.b_content[i].b_name);
                    }
                    break;
                }
            }
        }
        
        // Si no encuentra en raíz, usar raíz como default
        if (targetInode < 0) {
            targetInode = 0;
            targetDirName = "/";
        }
        
        Inodo targetIno;
        if (!DiskManager::readInodo(diskPath, part->part_start, targetInode, targetIno)) {
            return "Error: No se pudo leer el inodo del directorio.";
        }
        
        // El reporte ls funciona con directorios
        if (targetIno.i_type != 0) {
            return "Error: La ruta especificada no es un directorio.";
        }
        
        graphvizContent = "digraph DirectoryListing {\n";
        graphvizContent += "  rankdir=TB;\n";
        graphvizContent += "  node [shape=plaintext];\n";
        graphvizContent += "  DirList [label=<\n";
        graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\">\n";
        
        // Encabezado
        graphvizContent += "      <TR><TD COLSPAN=\"8\" BGCOLOR=\"#228B22\"><B><FONT COLOR=\"white\">Listado de: " + targetDirName + "</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR>\n";
        graphvizContent += "        <TD><B>Permisos</B></TD>\n";
        graphvizContent += "        <TD><B>Owner</B></TD>\n";
        graphvizContent += "        <TD><B>Grupo</B></TD>\n";
        graphvizContent += "        <TD><B>Size (Bytes)</B></TD>\n";
        graphvizContent += "        <TD><B>Fecha</B></TD>\n";
        graphvizContent += "        <TD><B>Hora</B></TD>\n";
        graphvizContent += "        <TD><B>Tipo</B></TD>\n";
        graphvizContent += "        <TD><B>Name</B></TD>\n";
        graphvizContent += "      </TR>\n";
        
        // Leer las entradas del directorio desde sus bloques
        for (int blockIdx = 0; blockIdx < 15 && targetIno.i_block[blockIdx] != -1; blockIdx++) {
            BlockFolder dirBlock;
            if (DiskManager::readBlock(diskPath, part->part_start, targetIno.i_block[blockIdx], 
                                      (char*)&dirBlock, sizeof(BlockFolder))) {
                
                for (int entryIdx = 0; entryIdx < 4; entryIdx++) {
                    if (dirBlock.b_content[entryIdx].b_inodo > 0) {
                        // Leer el inodo para obtener información detallada
                        Inodo entryIno;
                        std::string typeStr = "?";
                        std::string permStr = "---------";
                        std::string dateStr = "?";
                        std::string timeStr = "?";
                        std::string ownerStr = "?";
                        std::string groupStr = "?";
                        int sizeVal = 0;
                        
                        if (DiskManager::readInodo(diskPath, part->part_start, 
                                                   dirBlock.b_content[entryIdx].b_inodo, entryIno)) {
                            
                            // Tipo
                            typeStr = (entryIno.i_type == 0) ? "Carpeta" : "Archivo";
                            
                            // Permisos (convertir de octal a formato -rwx)
                            // Los permisos están en formato de 3 caracteres: usuario, grupo, otro
                            std::string permChars = std::string(entryIno.i_perm);  // ej: "664"
                            std::string permFormat = "-";  // por defecto archivo
                            if (entryIno.i_type == 0) {
                                permFormat = "d";  // directorio
                            }
                            
                            // Convertir cada dígito a rwx
                            for (int p = 0; p < 3 && p < (int)permChars.length(); p++) {
                                int digit = permChars[p] - '0';
                                permFormat += ((digit & 4) ? 'r' : '-');
                                permFormat += ((digit & 2) ? 'w' : '-');
                                permFormat += ((digit & 1) ? 'x' : '-');
                            }
                            permStr = permFormat;
                            
                            // Owner y Grupo
                            ownerStr = "User" + std::to_string(entryIno.i_uid);
                            groupStr = "Group" + std::to_string(entryIno.i_gid);
                            
                            // Tamaño
                            sizeVal = entryIno.i_s;
                            
                            // Fecha y Hora
                            time_t modTime = entryIno.i_mtime;
                            struct tm* timeinfo = localtime(&modTime);
                            char dateBuffer[20];
                            char timeBuffer[20];
                            strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%Y", timeinfo);
                            strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", timeinfo);
                            dateStr = std::string(dateBuffer);
                            timeStr = std::string(timeBuffer);
                        }
                        
                        // Agregar fila
                        graphvizContent += "      <TR>\n";
                        graphvizContent += "        <TD><FONT FACE=\"Courier\">" + permStr + "</FONT></TD>\n";
                        graphvizContent += "        <TD>" + ownerStr + "</TD>\n";
                        graphvizContent += "        <TD>" + groupStr + "</TD>\n";
                        graphvizContent += "        <TD>" + std::to_string(sizeVal) + "</TD>\n";
                        graphvizContent += "        <TD>" + dateStr + "</TD>\n";
                        graphvizContent += "        <TD>" + timeStr + "</TD>\n";
                        graphvizContent += "        <TD>" + typeStr + "</TD>\n";
                        graphvizContent += "        <TD>" + std::string(dirBlock.b_content[entryIdx].b_name) + "</TD>\n";
                        graphvizContent += "      </TR>\n";
                    }
                }
            }
        }
        
        graphvizContent += "    </TABLE>\n";
        graphvizContent += "  >];\n";
        graphvizContent += "}\n";
        
    } else if (repName == "disk") {
        // Reporte de estructura del disco con MBR y porcentajes
        int diskSize = (int)DiskManager::getFileSize(diskPath);
        
        // Recolectar información de particiones
        struct PartInfo {
            std::string name;
            int start;
            int size;
            char type;
            char status;
            int index;
        };
        std::vector<PartInfo> partitions;
        
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                partitions.push_back({
                    std::string(mbr.mbr_partitions[i].part_name),
                    mbr.mbr_partitions[i].part_start,
                    mbr.mbr_partitions[i].part_s,
                    mbr.mbr_partitions[i].part_type,
                    mbr.mbr_partitions[i].part_status,
                    i
                });
            }
        }
        
        // Calcular espacios libres
        std::vector<PartInfo> freeSpaces;
        int currentPos = sizeof(MBR);
        for (const auto& part : partitions) {
            if (part.start > currentPos) {
                freeSpaces.push_back({
                    "Libre",
                    currentPos,
                    part.start - currentPos,
                    'F',
                    '0',
                    -1
                });
            }
            currentPos = part.start + part.size;
        }
        
        if (currentPos < diskSize) {
            freeSpaces.push_back({
                "Libre",
                currentPos,
                diskSize - currentPos,
                'F',
                '0',
                -1
            });
        }
        
        // Generar reporte
        graphvizContent = "digraph Disk {\n";
        graphvizContent += "  rankdir=TB;\n";
        graphvizContent += "  node [shape=plaintext];\n";
        graphvizContent += "  DiskReport [label=<\n";
        graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\">\n";
        
        // Encabezado
        graphvizContent += "      <TR><TD COLSPAN=\"7\" BGCOLOR=\"#800080\"><B><FONT COLOR=\"white\">REPORTE DE DISCO</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD>mbr_tamano</TD><TD COLSPAN=\"6\">" + std::to_string(diskSize) + " bytes</TD></TR>\n";
        graphvizContent += "      <TR><TD>mbr_fecha_creacion</TD><TD COLSPAN=\"6\">" + std::string(__DATE__) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD>mbr_dsk_signature</TD><TD COLSPAN=\"6\">" + std::to_string(mbr.mbr_dsk_signature) + "</TD></TR>\n";
        graphvizContent += "      <TR><TD COLSPAN=\"7\"></TD></TR>\n";
        
        // Tabla de particiones con porcentajes
        graphvizContent += "      <TR><TD COLSPAN=\"7\" BGCOLOR=\"#A020F0\"><B><FONT COLOR=\"white\">Particiones del Disco</FONT></B></TD></TR>\n";
        graphvizContent += "      <TR><TD><B>Nombre</B></TD><TD><B>Tipo</B></TD><TD><B>Inicio</B></TD><TD><B>Tamaño</B></TD><TD><B>Bytes</B></TD><TD><B>Porcentaje</B></TD><TD><B>Barra</B></TD></TR>\n";
        
        // Mostrar particiones
        for (const auto& part : partitions) {
            double percentage = (diskSize > 0) ? (part.size * 100.0 / diskSize) : 0;
            int barLength = (int)(percentage / 2);  // 50 caracteres max = 100%
            std::string bar = "";
            for (int i = 0; i < barLength; i++) bar += "█";
            
            std::string typeStr = "";
            std::string bgColor = "#E8E8E8";
            if (part.type == 'P') { typeStr = "Primaria"; bgColor = "#FFE4B5"; }
            else if (part.type == 'E') { typeStr = "Extendida"; bgColor = "#FFB6C1"; }
            else if (part.type == 'L') { typeStr = "Lógica"; bgColor = "#ADD8E6"; }
            
            graphvizContent += "      <TR><TD>" + part.name + "</TD>";
            graphvizContent += "<TD>" + typeStr + "</TD>";
            graphvizContent += "<TD>" + std::to_string(part.start) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(part.size) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(part.size) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(percentage).substr(0, 5) + "%</TD>";
            graphvizContent += "<TD BGCOLOR=\"" + bgColor + "\">" + bar + "</TD></TR>\n";
        }
        
        // Mostrar espacios libres
        for (const auto& freeSpace : freeSpaces) {
            double percentage = (diskSize > 0) ? (freeSpace.size * 100.0 / diskSize) : 0;
            int barLength = (int)(percentage / 2);
            std::string bar = "";
            for (int i = 0; i < barLength; i++) bar += "░";
            
            graphvizContent += "      <TR><TD>" + freeSpace.name + "</TD>";
            graphvizContent += "<TD>-</TD>";
            graphvizContent += "<TD>" + std::to_string(freeSpace.start) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(freeSpace.size) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(freeSpace.size) + "</TD>";
            graphvizContent += "<TD>" + std::to_string(percentage).substr(0, 5) + "%</TD>";
            graphvizContent += "<TD BGCOLOR=\"#D3D3D3\">" + bar + "</TD></TR>\n";
        }
        
        // Fila de totales
        double totalUsed = 0;
        for (const auto& part : partitions) totalUsed += part.size;
        double totalPercentage = (diskSize > 0) ? (totalUsed * 100.0 / diskSize) : 0;
        
        graphvizContent += "      <TR><TD COLSPAN=\"7\"></TD></TR>\n";
        graphvizContent += "      <TR>\n";
        graphvizContent += "        <TD><B>TOTAL USADO</B></TD>\n";
        graphvizContent += "        <TD>-</TD>\n";
        graphvizContent += "        <TD>-</TD>\n";
        graphvizContent += "        <TD>" + std::to_string((int)totalUsed) + "</TD>\n";
        graphvizContent += "        <TD>" + std::to_string(diskSize) + "</TD>\n";
        graphvizContent += "        <TD><B>" + std::to_string(totalPercentage).substr(0, 5) + "%</B></TD>\n";
        graphvizContent += "        <TD>-</TD>\n";
        graphvizContent += "      </TR>\n";
        graphvizContent += "    </TABLE>\n";
        graphvizContent += "  >];\n";
        graphvizContent += "}\n";
        
    } else if (repName == "ebr") {
        // Reporte de EBR (Particiones Lógicas)
        // Verificar que la partición es extendida
        if (part->part_type != 'E') {
            return "Error: La partición no es extendida. No hay EBRs para este tipo de partición.";
        }
        
        graphvizContent = "digraph EBR {\n";
        graphvizContent += "  rankdir=TB;\n";
        graphvizContent += "  node [shape=plaintext];\n";
        
        // Leer EBRs de la cadena de particiones lógicas
        int ebr_offset = part->part_start;
        bool first = true;
        
        while (ebr_offset > 0) {
            EBR ebr;
            if (!DiskManager::readFromDisk(diskPath, ebr_offset, (char*)&ebr, sizeof(EBR))) {
                break;
            }
            
            if (ebr.part_s == 0 && ebr.part_start < 0) {
                break;  // EBR vacío
            }
            
            // Crear tabla para esta partición
            std::string nodeId = "ebr_" + std::to_string(ebr_offset);
            graphvizContent += "  " + nodeId + " [label=<\n";
            graphvizContent += "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" BGCOLOR=\"#E8E8E8\">\n";
            graphvizContent += "      <TR><TD COLSPAN=\"2\" BGCOLOR=\"#800080\"><B><FONT COLOR=\"white\">Partición</FONT></B></TD></TR>\n";
            graphvizContent += "      <TR><TD>part_mount</TD><TD>" + std::string(1, ebr.part_mount) + "</TD></TR>\n";
            graphvizContent += "      <TR><TD>part_type</TD><TD>L</TD></TR>\n";
            graphvizContent += "      <TR><TD>part_fit</TD><TD>" + std::string(1, ebr.part_fit) + "</TD></TR>\n";
            graphvizContent += "      <TR><TD>part_start</TD><TD>" + std::to_string(ebr.part_start) + "</TD></TR>\n";
            graphvizContent += "      <TR><TD>part_size</TD><TD>" + std::to_string(ebr.part_s) + "</TD></TR>\n";
            graphvizContent += "      <TR><TD>part_name</TD><TD>" + std::string(ebr.part_name) + "</TD></TR>\n";
            graphvizContent += "    </TABLE>\n";
            graphvizContent += "  >];\n";
            
            // Apuntar al siguiente EBR si existe
            if (ebr.part_next > 0) {
                std::string nextNodeId = "ebr_" + std::to_string(ebr.part_next);
                graphvizContent += "  " + nodeId + " -> " + nextNodeId + " [label=\"next\"];\n";
            }
            
            ebr_offset = ebr.part_next;
        }
        
        graphvizContent += "}\n";
    }
    
    // Guardar archivo .dot
    std::string dotFilePath = repPath;
    if (dotFilePath.find(".dot") == std::string::npos) {
        dotFilePath += ".dot";
    }
    
    std::ofstream dotFile(dotFilePath);
    if (!dotFile.is_open()) {
        return "Error: No se pudo crear el archivo de reporte: " + dotFilePath;
    }
    
    dotFile << graphvizContent;
    dotFile.close();
    
    return "Reporte " + repName + " generado en: " + dotFilePath;
}

// ============ FUNCIONES HELPER PARA FILESYSTEM ============

bool CommandHandler::getInodeFromNum(const std::string& diskPath, int partStart, int inodeNum, Inodo& ino) {
    return DiskManager::readInodo(diskPath, partStart, inodeNum, ino);
}

int CommandHandler::findInodeInDirectory(const std::string& diskPath, int partStart, int parentInode,
                                        const std::string& name, Inodo& parentIno) {
    // Leer el inodo del padre
    if (!DiskManager::readInodo(diskPath, partStart, parentInode, parentIno)) {
        return -1;
    }
    
    // Leer los bloques del directorio padre
    for (int i = 0; i < 15 && parentIno.i_block[i] != -1; i++) {
        BlockFolder dirBlock;
        if (!DiskManager::readBlock(diskPath, partStart, parentIno.i_block[i], 
                                   (char*)&dirBlock, sizeof(BlockFolder))) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (dirBlock.b_content[j].b_inodo != 0 && 
                std::string(dirBlock.b_content[j].b_name) == name) {
                return dirBlock.b_content[j].b_inodo;
            }
        }
    }
    
    return -1;
}

int CommandHandler::getInodeFromPath(const std::string& diskPath, int partStart, 
                                     const std::string& path, Inodo& resultIno, char& type) {
    // Parsear la ruta
    std::vector<std::string> parts;
    std::string current = "";
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) {
                parts.push_back(current);
                current = "";
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    
    // Leer superblock
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return -1;
    }
    
    // Empezar desde raíz (inodo 0)
    int currentInode = 0;
    Inodo currentInoData;
    
    // Navegar la ruta
    for (size_t i = 0; i < parts.size(); i++) {
        if (!DiskManager::readInodo(diskPath, partStart, currentInode, currentInoData)) {
            return -1;
        }
        
        if (i == parts.size() - 1) {
            // Última parte - es el target
            resultIno = currentInoData;
            type = currentInoData.i_type;
            return currentInode;
        }
        
        // No es la última, debe ser un directorio
        if (currentInoData.i_type != 0) {
            return -1;  // Ruta inválida, no es directorio
        }
        
        // Buscar en el directorio actual
        int nextInode = findInodeInDirectory(diskPath, partStart, currentInode, parts[i], currentInoData);
        if (nextInode < 0) {
            return -1;  // Carpeta no encontrada
        }
        
        currentInode = nextInode;
    }
    
    return currentInode;
}

bool CommandHandler::createParentDirs(const std::string& diskPath, int partStart, int superblockStart,
                                      const std::string& path) {
    // Parsear la ruta para obtener las carpetas padres
    std::vector<std::string> parts;
    std::string current = "";
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) {
                parts.push_back(current);
                current = "";
            }
        } else {
            current += c;
        }
    }
    // No incluir el último elemento (el archivo/carpeta a crear)
    parts.pop_back();
    
    Superblock sb;
    if (!DiskManager::readSuperblock(diskPath, partStart, sb)) {
        return false;
    }
    
    // Empezar desde raíz
    int currentInode = 0;
    Inodo currentInoData;
    
    // Crear cada carpeta padre que no existe
    for (size_t i = 0; i < parts.size(); i++) {
        if (!DiskManager::readInodo(diskPath, partStart, currentInode, currentInoData)) {
            return false;
        }
        
        // Buscar si ya existe
        int existingInode = findInodeInDirectory(diskPath, partStart, currentInode, parts[i], currentInoData);
        if (existingInode >= 0) {
            // Ya existe, continuar
            currentInode = existingInode;
            continue;
        }
        
        // Crear nueva carpeta
        int newInode = DiskManager::allocateInode(diskPath, partStart, sb);
        if (newInode < 0) {
            return false;
        }
        
        int dirBlock = DiskManager::allocateBlock(diskPath, partStart, sb);
        if (dirBlock < 0) {
            DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
            return false;
        }
        
        // Crear inodo del directorio
        Inodo newDirIno;
        newDirIno.i_uid = 0;
        newDirIno.i_gid = 0;
        newDirIno.i_s = 0;
        newDirIno.i_atime = time(nullptr);
        newDirIno.i_ctime = time(nullptr);
        newDirIno.i_mtime = time(nullptr);
        newDirIno.i_type = 0;  // 0 = carpeta (directorio)
        std::strncpy(newDirIno.i_perm, "664", 2);
        
        for (int j = 0; j < 15; j++) {
            newDirIno.i_block[j] = (j == 0) ? dirBlock : -1;
        }
        
        // Inicializar bloque del directorio con "." y ".."
        BlockFolder newDirBlock;
        // Primer registro: "." -> apunta al inodo del directorio actual
        std::strncpy(newDirBlock.b_content[0].b_name, ".", 11);
        newDirBlock.b_content[0].b_inodo = newInode;
        
        // Segundo registro: ".." -> apunta al directorio padre
        std::strncpy(newDirBlock.b_content[1].b_name, "..", 11);
        newDirBlock.b_content[1].b_inodo = currentInode;
        
        // Inicializar registros restantes como vacíos
        newDirBlock.b_content[2].b_inodo = 0;
        newDirBlock.b_content[3].b_inodo = 0;
        std::memset(newDirBlock.b_content[2].b_name, 0, 12);
        std::memset(newDirBlock.b_content[3].b_name, 0, 12);
        
        // Escribir bloque del directorio
        DiskManager::writeBlock(diskPath, partStart, dirBlock, (char*)&newDirBlock, sizeof(BlockFolder));
        
        if (!DiskManager::writeInodo(diskPath, partStart, newInode, newDirIno)) {
            DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
            return false;
        }
        
        // Leer el directorio padre
        Inodo parentDirIno;
        if (!DiskManager::readInodo(diskPath, partStart, currentInode, parentDirIno)) {
            DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
            return false;
        }
        
        // Agregar entrada en el directorio padre
        bool added = false;
        for (int j = 0; j < 15 && parentDirIno.i_block[j] != -1; j++) {
            BlockFolder folderBlock;
            if (!DiskManager::readBlock(diskPath, partStart, parentDirIno.i_block[j],
                                       (char*)&folderBlock, sizeof(BlockFolder))) {
                continue;
            }
            
            for (int k = 0; k < 4; k++) {
                if (folderBlock.b_content[k].b_inodo == 0) {
                    std::strncpy(folderBlock.b_content[k].b_name, parts[i].c_str(), 11);
                    folderBlock.b_content[k].b_inodo = newInode;
                    
                    if (!DiskManager::writeBlock(diskPath, partStart, parentDirIno.i_block[j],
                                               (char*)&folderBlock, sizeof(BlockFolder))) {
                        DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
                        DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
                        return false;
                    }
                    added = true;
                    break;
                }
            }
            if (added) break;
        }
        
        if (!added) {
            DiskManager::deallocateBlock(diskPath, partStart, dirBlock, sb);
            DiskManager::deallocateInode(diskPath, partStart, newInode, sb);
            return false;
        }
        
        currentInode = newInode;
    }
    
    return true;
}

bool CommandHandler::hasReadPermission(const Inodo& ino, const std::string& userName) {
    // ROOT siempre tiene permisos
    if (userName == "root") {
        return true;
    }
    
    // Para este proyecto simplificado, permitir lectura
    // En proyecto 2 se implementará validación de permisos UGO completa
    return true;
}

bool CommandHandler::hasWritePermission(const Inodo& ino, const std::string& userName) {
    // ROOT siempre tiene permisos
    if (userName == "root") {
        return true;
    }
    
    // Para este proyecto simplificado, permitir escritura
    // En proyecto 2 se implementará validación de permisos UGO completa
    return true;
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
