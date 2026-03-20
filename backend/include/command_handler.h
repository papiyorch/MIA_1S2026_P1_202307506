#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <map>
#include <vector>
#include "structures.h"
#include "disk_manager.h"

class CommandHandler {
private:
    std::map<std::string, std::string> params;
    std::string currentUser;
    std::string currentPartitionId;
    bool isLoggedIn;
    std::map<std::string, std::string> mountedPartitions;  // ID -> ruta del disco
    std::map<std::string, int> diskMountStates;  // ruta del disco -> número de montaje
    std::map<std::string, char> diskLetterStates;  // ruta del disco -> letra actual (A-D)
    std::map<std::string, std::string> groups;  // groupName -> groupId
    std::map<std::string, std::string> users;   // userName -> password:groupName

public:
    CommandHandler();
    
    // Procesa un comando completo
    std::string processCommand(const std::string& line);
    
    // Obtiene el usuario actual
    std::string getCurrentUser() const;
    
    // Obtiene el ID de partición actual
    std::string getCurrentPartitionId() const;

private:
    // Comandos de disco
    std::string cmdMkdisk(const std::map<std::string, std::string>& params);
    std::string cmdRmdisk(const std::map<std::string, std::string>& params);
    std::string cmdFdisk(const std::map<std::string, std::string>& params);
    std::string cmdMount(const std::map<std::string, std::string>& params);
    std::string cmdMounted(const std::map<std::string, std::string>& params);
    
    // Comandos del sistema de archivos
    std::string cmdMkfs(const std::map<std::string, std::string>& params);
    std::string cmdLogin(const std::map<std::string, std::string>& params);
    std::string cmdLogout(const std::map<std::string, std::string>& params);
    std::string cmdCat(const std::map<std::string, std::string>& params);
    
    // Comandos de usuarios y grupos
    std::string cmdMkgrp(const std::map<std::string, std::string>& params);
    std::string cmdRmgrp(const std::map<std::string, std::string>& params);
    std::string cmdMkusr(const std::map<std::string, std::string>& params);
    std::string cmdRmusr(const std::map<std::string, std::string>& params);
    std::string cmdChgrp(const std::map<std::string, std::string>& params);
    
    // Comandos de archivos y carpetas
    std::string cmdMkdir(const std::map<std::string, std::string>& params);
    std::string cmdMkfile(const std::map<std::string, std::string>& params);
    
    // Reportes
    std::string cmdRep(const std::map<std::string, std::string>& params);
    
    // Funciones helper para filesystem
    struct PathNode {
        std::string fileName;
        int inodeNum;
        char type;  
    };
    
    // Obtiene un inodo por número
    bool getInodeFromNum(const std::string& diskPath, int partStart, int inodeNum, Inodo& ino);
    
    // Busca un archivo/carpeta por nombre en un directorio
    int findInodeInDirectory(const std::string& diskPath, int partStart, int parentInode, 
                            const std::string& name, Inodo& parentIno);
    
    // Obtiene el inodo de una ruta completa (ej: /home/user/file.txt)
    int getInodeFromPath(const std::string& diskPath, int partStart, const std::string& path,
                        Inodo& resultIno, char& type);
    
    // Crea recursivamente las carpetas padres de una ruta
    bool createParentDirs(const std::string& diskPath, int partStart, int superblockStart,
                         const std::string& path);
    
    // Valida si el usuario tiene permiso de lectura
    bool hasReadPermission(const Inodo& ino, const std::string& userName);
    
    // Valida si el usuario tiene permiso de escritura
    bool hasWritePermission(const Inodo& ino, const std::string& userName);
    
    // Valida parámetros obligatorios
    std::string validateMandatoryParams(const std::map<std::string, std::string>& params,
                                       const std::vector<std::string>& mandatory);
};

#endif // COMMAND_HANDLER_H
