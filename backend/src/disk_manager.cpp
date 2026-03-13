#include "disk_manager.h"
#include <filesystem>
#include <random>
#include <ctime>

namespace fs = std::filesystem;

bool DiskManager::createDisk(const std::string& path, int size, char fit) {
    // Crear directorios si no existen
    if (!createDirectories(path)) {
        return false;
    }

    // Verificar que el tamaño sea positivo
    if (size <= sizeof(MBR)) {
        std::cerr << "Error: Tamaño de disco menor que MBR" << std::endl;
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo crear archivo: " << path << std::endl;
        return false;
    }

    // Crear MBR inicial
    MBR mbr;
    mbr.mbr_tamano = size;
    mbr.mbr_fecha_creacion = time(nullptr);
    
    // Generar número aleatorio único
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 2147483647);
    mbr.mbr_dsk_signature = dis(gen);
    
    mbr.dsk_fit = fit;

    // Inicializar particiones
    for (int i = 0; i < 4; i++) {
        mbr.mbr_partitions[i].part_status = '0';
        mbr.mbr_partitions[i].part_type = 'N';
        mbr.mbr_partitions[i].part_fit = 'N';
        mbr.mbr_partitions[i].part_start = -1;
        mbr.mbr_partitions[i].part_s = 0;
        mbr.mbr_partitions[i].part_correlative = -1;
        std::strcpy(mbr.mbr_partitions[i].part_name, "");
        std::strcpy(mbr.mbr_partitions[i].part_id, "");
    }

    // Escribir MBR al inicio
    file.seekp(0);
    file.write((char*)&mbr, sizeof(MBR));

    if (!file.good()) {
        file.close();
        std::cerr << "Error: No se pudo escribir MBR" << std::endl;
        return false;
    }

    // Llenar resto con ceros usando buffer de 1024 bytes
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));
    
    int bytesWritten = sizeof(MBR);
    while (bytesWritten < size) {
        int remaining = size - bytesWritten;
        int toWrite = (remaining < 1024) ? remaining : 1024;
        file.write(buffer, toWrite);
        bytesWritten += toWrite;
    }

    file.close();

    if (!file.good()) {
        std::cerr << "Error: No se pudo escribir completamente el archivo" << std::endl;
        return false;
    }

    std::cout << "Disco creado exitosamente: " << path << std::endl;
    return true;
}

bool DiskManager::removeDisk(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            std::cerr << "Error: Archivo no existe: " << path << std::endl;
            return false;
        }
        fs::remove(path);
        std::cout << "Disco eliminado: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error al eliminar disco: " << e.what() << std::endl;
        return false;
    }
}

bool DiskManager::readFromDisk(const std::string& path, int offset, char* buffer, int size) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir archivo: " << path << std::endl;
        return false;
    }

    file.seekg(offset);
    file.read(buffer, size);

    if (!file.good()) {
        std::cerr << "Error: No se pudo leer del disco" << std::endl;
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool DiskManager::writeToDisk(const std::string& path, int offset, const char* buffer, int size) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir archivo: " << path << std::endl;
        return false;
    }

    file.seekp(offset);
    file.write(buffer, size);

    if (!file.good()) {
        std::cerr << "Error: No se pudo escribir en el disco" << std::endl;
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool DiskManager::readMBR(const std::string& path, MBR& mbr) {
    return readFromDisk(path, 0, (char*)&mbr, sizeof(MBR));
}

bool DiskManager::writeMBR(const std::string& path, const MBR& mbr) {
    return writeToDisk(path, 0, (char*)&mbr, sizeof(MBR));
}

bool DiskManager::fileExists(const std::string& path) {
    return fs::exists(path);
}

long DiskManager::getFileSize(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            return -1;
        }
        return fs::file_size(path);
    } catch (const std::exception& e) {
        std::cerr << "Error obteniendo tamaño: " << e.what() << std::endl;
        return -1;
    }
}

bool DiskManager::createDirectories(const std::string& path) {
    try {
        fs::path p(path);
        fs::path parent = p.parent_path();
        
        if (!parent.empty() && !fs::exists(parent)) {
            fs::create_directories(parent);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creando directorios: " << e.what() << std::endl;
        return false;
    }
}
