/**
 * @file eefile.cpp
 * @brief 最小化 EEPROM 管理 - 只存储原始数据 + 启动标志位
 * @note 去掉了 CRC 和数据头，节省空间；支持多扇区
 */

#include "Eefile.h"
#include "Debug.h"
#include <cstdarg>

// ============ 通过枚举查找文件索引 ============
int8_t EEFILE::findFileIndex(EEFileType type)
{
    for (uint8_t i = 0; i < fileCount; i++) {
        if (files[i].type == type) {
            return i;
        }
    }
    return -1;
}

// ============ 计算下一个可用地址 ============
uint16_t EEFILE::calculateNextAddr(void)
{
    if (fileCount == 0) {
        return 0;  // 从 0 开始
    }

    // 获取最后一个文件的结束地址
    uint16_t lastEnd = files[fileCount - 1].endAddr;

    // 下一个文件从最后一个文件的结束地址 + 1 开始
    return lastEnd + 1;
}

// ============ Constructor ============
EEFILE::EEFILE()
    : fileCount(0), is_enabled(false)
{
    memset(files, 0, sizeof(files));
}

// ============ 初始化 EEPROM ============
void EEFILE::begin()
{
    ::EEPROM.begin();
    is_enabled = true;
    FILE_DEBUG("[EEFILE] EEPROM initialized");
    FILE_DEBUG("[EEFILE] Total: %d bytes (%d sectors × %d)",
        EEFILE_TOTAL_SIZE, EEFILE_NUM_SECTORS, EEFILE_SECTOR_SIZE);
}

// ============ 启用/禁用 EEPROM ============
void EEFILE::enable()
{
    is_enabled = true;
    FILE_DEBUG("[EEFILE] EEPROM enabled");
}

void EEFILE::disable()
{
    is_enabled = false;
    FILE_DEBUG("[EEFILE] EEPROM disabled");
}

bool EEFILE::isEnabled() const
{
    return is_enabled;
}

// ============ 自动注册文件 ============
// 注意：实际占用空间 = maxSize + 1（第一个字节是有效性标记）
bool EEFILE::registerAuto(EEFileType type, uint16_t maxSize)
{
    // 检查是否已超过最大文件数
    if (fileCount >= EEFILE_MAX_FILES) {
        FILE_DEBUG("[EE] ERROR: Max files (%d) reached!", EEFILE_MAX_FILES);
        return false;
    }

    // 检查该类型是否已注册
    if (findFileIndex(type) != -1) {
        FILE_DEBUG("[EE] ERROR: Type %d already registered!", type);
        return false;
    }

    // 检查总空间是否足够（需要 maxSize + 1 字节用于有效性标记）
    uint16_t nextAddr = calculateNextAddr();
    uint16_t actualSize = maxSize + 1;  // +1 用于有效性标记
    if (nextAddr + actualSize > EEFILE_TOTAL_SIZE) {
        FILE_DEBUG("[EE] ERROR: Not enough space (need %d, available %d)",
            actualSize, EEFILE_TOTAL_SIZE - nextAddr);
        return false;
    }

    // 注册文件
    files[fileCount].type = type;
    files[fileCount].maxSize = maxSize;  // 用户数据大小（不包括有效性标记）
    files[fileCount].startAddr = nextAddr;  // 有效性标记所在地址
    files[fileCount].endAddr = nextAddr + actualSize - 1;  // 包含有效性标记
    files[fileCount].dataLen = 0;
    files[fileCount].enabled = true;
    files[fileCount].modified = false;

    FILE_DEBUG("[EE] Type %d: 0x%04X-0x%04X (%d+1 bytes) [data: 0x%04X]",
        type, nextAddr, nextAddr + actualSize - 1, maxSize, nextAddr + 1);

    fileCount++;
    return true;
}

// ============ 写入数据 ============
// 存储格式：[有效性标记(0x01)] + [用户数据] + [填充0xFF]
bool EEFILE::write(EEFileType type, const uint8_t* data, uint16_t length)
{
    // 检查 EEPROM 是否启用
    if (!is_enabled) {
        FILE_DEBUG("[EE] ERROR: EEPROM disabled");
        return false;
    }

    // 查找文件
    int8_t idx = findFileIndex(type);
    if (idx == -1) {
        FILE_DEBUG("[EE] ERROR: Type %d not found", type);
        return false;
    }

    // 检查文件是否启用
    if (!files[idx].enabled) {
        FILE_DEBUG("[EE] ERROR: Type %d disabled", type);
        return false;
    }

    // 检查数据长度
    if (length > files[idx].maxSize) {
        FILE_DEBUG("[EE] ERROR: Data %d > max %d", length, files[idx].maxSize);
        return false;
    }

    uint16_t address = files[idx].startAddr;
    uint16_t dataAddr = address + 1;  // 数据从第二个字节开始

    // ============ 关键设计：第一个字节是有效性标记 ============
    // 1. 先写有效性标记（0x01 表示有效）
    ::EEPROM.write(address, 0x01);

    // 2. 写入实际数据（从 address+1 开始）
    for (uint16_t i = 0; i < length; i++) {
        ::EEPROM.write(dataAddr + i, data[i]);
    }

    // 3. 填充剩余空间为 0xFF
    for (uint16_t i = length; i < files[idx].maxSize; i++) {
        ::EEPROM.write(dataAddr + i, 0xFF);
    }

    // 更新元数据
    files[idx].dataLen = length;
    files[idx].modified = true;

    FILE_DEBUG("[EE] Type %d: wrote %d bytes (addr: 0x%04X, marker: 0x01)",
        type, length, address);

    return true;
}

// ============ 读取数据 ============
// 读取格式：先检查有效性标记(address+0)，再读用户数据(address+1起)
bool EEFILE::read(EEFileType type, uint8_t* data, uint16_t length)
{
    // 检查 EEPROM 是否启用
    if (!is_enabled) {
        FILE_DEBUG("[EE] ERROR: EEPROM disabled");
        return false;
    }

    // 查找文件
    int8_t idx = findFileIndex(type);
    if (idx == -1) {
        FILE_DEBUG("[EE] ERROR: Type %d not found", type);
        return false;
    }

    // 检查文件是否启用
    if (!files[idx].enabled) {
        FILE_DEBUG("[EE] ERROR: Type %d disabled", type);
        return false;
    }

    uint16_t address = files[idx].startAddr;
    uint16_t dataAddr = address + 1;  // 数据从第二个字节开始

    // ============ 关键检查：读取有效性标记 ============
    uint8_t validMarker = ::EEPROM.read(address);
    if (validMarker != 0x01) {
        FILE_DEBUG("[EE] ERROR: Type %d data invalid (marker: 0x%02X)",
            type, validMarker);
        return false;
    }

    // 检查数据长度
    if (length != files[idx].dataLen) {
        FILE_DEBUG("[EE] WARNING: Type %d expected %d, got %d",
            type, files[idx].dataLen, length);
    }

    // 读取用户数据（从 address+1 开始）
    uint16_t readLen = (length < files[idx].dataLen) ? length : files[idx].dataLen;
    for (uint16_t i = 0; i < readLen; i++) {
        data[i] = ::EEPROM.read(dataAddr + i);
    }

    FILE_DEBUG("[EE] Type %d: read %d bytes (marker: 0x%02X)",
        type, readLen, validMarker);

    return true;
}

// ============ 清除文件 ============
// 只需将有效性标记设置为 0x00，数据部分不必清除
bool EEFILE::erase(EEFileType type)
{
    // 检查 EEPROM 是否启用
    if (!is_enabled) {
        FILE_DEBUG("[EE] ERROR: EEPROM disabled");
        return false;
    }

    // 查找文件
    int8_t idx = findFileIndex(type);
    if (idx == -1) {
        FILE_DEBUG("[EE] ERROR: Type %d not found", type);
        return false;
    }

    uint16_t address = files[idx].startAddr;

    // 只需将有效性标记设置为 0x00（表示无效）
    // 这样下次读取时会检查到标记无效，而不需要清除所有数据
    ::EEPROM.write(address, 0x00);

    // 重置元数据
    files[idx].dataLen = 0;
    files[idx].modified = false;

    FILE_DEBUG("[EE] Type %d erased (marker: 0x00)", type);

    return true;
}

// ============ 启用/禁用文件 ============
void EEFILE::setFileEnabled(EEFileType type, bool enabled)
{
    int8_t idx = findFileIndex(type);
    if (idx != -1) {
        files[idx].enabled = enabled;
        FILE_DEBUG("[EE] Type %d: %s", type, enabled ? "enabled" : "disabled");
    }
}

bool EEFILE::isFileEnabled(EEFileType type)
{
    int8_t idx = findFileIndex(type);
    return (idx != -1) ? files[idx].enabled : false;
}

// ============ 获取文件数据长度 ============
uint16_t EEFILE::getFileDataLen(EEFileType type)
{
    int8_t idx = findFileIndex(type);
    return (idx != -1) ? files[idx].dataLen : 0;
}

// ============ 获取文件是否被修改 ============
bool EEFILE::isFileModified(EEFileType type)
{
    int8_t idx = findFileIndex(type);
    return (idx != -1) ? files[idx].modified : false;
}

// ============ 清除修改标志 ============
void EEFILE::clearModifiedFlag(EEFileType type)
{
    int8_t idx = findFileIndex(type);
    if (idx != -1) {
        files[idx].modified = false;
        FILE_DEBUG("[EE] Type %d: modified flag cleared", type);
    }
}

// ============ 检查文件有效性（从 Flash 读取标记）============
bool EEFILE::isFileValid(EEFileType type)
{
    int8_t idx = findFileIndex(type);
    if (idx == -1) {
        return false;
    }

    uint16_t address = files[idx].startAddr;
    uint8_t validMarker = ::EEPROM.read(address);
    bool isValid = (validMarker == 0x01);

    FILE_DEBUG("[EE] Type %d: isValid=%s (marker: 0x%02X)",
        type, isValid ? "true" : "false", validMarker);

    return isValid;
}

// ============ 设置文件有效性标记（写入 Flash）============
void EEFILE::setFileValid(EEFileType type, bool valid)
{
    int8_t idx = findFileIndex(type);
    if (idx == -1) {
        FILE_DEBUG("[EE] ERROR: Type %d not found", type);
        return;
    }

    uint16_t address = files[idx].startAddr;
    uint8_t marker = valid ? 0x01 : 0x00;
    ::EEPROM.write(address, marker);

    FILE_DEBUG("[EE] Type %d: setValid=%s (marker: 0x%02X)",
        type, valid ? "true" : "false", marker);
}

// ============ 获取文件地址（调试用）============
uint16_t EEFILE::getFileAddr(EEFileType type)
{
    int8_t idx = findFileIndex(type);
    return (idx != -1) ? files[idx].startAddr : 0;
}

// ============ 打印全局状态 ============
void EEFILE::printStatus()
{
    FILE_DEBUG("\n====== EEFILE Status ======");
    FILE_DEBUG("Enabled: %s", is_enabled ? "Yes" : "No");
    FILE_DEBUG("Total: %d bytes (%d sectors)", EEFILE_TOTAL_SIZE, EEFILE_NUM_SECTORS);
    FILE_DEBUG("Registered: %d files\n", fileCount);

    for (uint8_t i = 0; i < fileCount; i++) {
        FILE_DEBUG("  Type %d: 0x%04X-%04X (%d bytes) [%s|%s|V%d]",
            files[i].type,
            files[i].startAddr,
            files[i].endAddr,
            files[i].dataLen,
            files[i].enabled ? "E" : "D",
            files[i].modified ? "M" : "C");
    }

    FILE_DEBUG("===========================\n");
}

// ============ 打印单个文件信息 ============
void EEFILE::printFileInfo(EEFileType type)
{
    int8_t idx = findFileIndex(type);
    if (idx == -1) {
        FILE_DEBUG("[EE] Type %d not found", type);
        return;
    }

    FILE_DEBUG("\n---- Type %d Info ----", type);
    FILE_DEBUG("Address: 0x%04X", files[idx].startAddr);
    FILE_DEBUG("Max size: %d bytes", files[idx].maxSize);
    FILE_DEBUG("Data len: %d bytes", files[idx].dataLen);
    FILE_DEBUG("Enabled: %s", files[idx].enabled ? "Yes" : "No");
    FILE_DEBUG("Modified: %s", files[idx].modified ? "Yes" : "No");
    FILE_DEBUG("--------------------\n");
}
