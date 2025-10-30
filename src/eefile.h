#ifndef __EEFILE__
#define __EEFILE__
#include "Arduino.h"
#include "EEPROM.h"

// ============ 用户定义：文件类型枚举 ============
// 用户只需定义要保存的数据类型，地址由系统自动管理
typedef enum{
    IIC_START = 0,   // I2C地址
    KAL_MAN,         // Kalman参数
    // 添加新类型时直接加在这里，无需关心地址
    END              // 必须以 END 结尾
} EEFileType;

// ============ 最小化文件元数据结构 ============
// 只保留必要信息，节省内存
// 注意：Flash 中实际存储格式为：[有效性标记(1字节)] + [用户数据]
typedef struct {
    EEFileType type;          // 文件类型
    uint16_t maxSize;         // 最大数据大小（字节，不包括有效性标记）
    uint16_t startAddr;       // 自动分配的起始地址（有效性标记的地址）
    uint16_t endAddr;         // 自动分配的结束地址
    uint16_t dataLen;         // 实际数据长度（不包括有效性标记）
    bool enabled;          // 是否启用
    bool modified;         // 是否内容改变
    // 注意：valid 标志现在存储在 Flash 的第一个字节，不再用内存中的字段
} FileMetadata;

// ============ EEPROM 扇区配置 ============
// 支持使用最后 N 个扇区
#define EEFILE_MAX_FILES 10                        // 最多支持 10 个文件
#define EEFILE_SECTOR_SIZE 256                     // 每个扇区 256 字节
#define EEFILE_NUM_SECTORS 2                       // 使用最后 2 个扇区
#define EEFILE_TOTAL_SIZE (EEFILE_SECTOR_SIZE * EEFILE_NUM_SECTORS)  // 总共 512 字节

// 注意：实际地址由系统自动计算，用户无需关心
// 地址从 0x00 开始（扇区 0），顺序分配

class EEFILE
{
  private:
    // 内部状态
    FileMetadata files[EEFILE_MAX_FILES];  // 文件元数据表
    uint8_t fileCount;                     // 已注册的文件数量
    bool is_enabled;                    // EEPROM 功能是否启用

    // 内部方法
    uint16_t calculateCRC(const uint8_t* data, uint16_t length);
    bool verifyCRC(const uint8_t* data, uint16_t length, uint16_t crc);
    int8_t findFileIndex(EEFileType type);
    uint16_t calculateNextAddr(void);

  public:
    // Constructor
    EEFILE();

    // 单例模式
    static EEFILE &getInstance(void)
    {
        static EEFILE eefile;
        return eefile;
    }

    // ========== 初始化 ==========
    void begin();

    // ========== 启用/禁用 ==========
    void enable();
    void disable();
    bool isEnabled() const;

    // ========== 自动地址注册（核心接口）==========
    /**
     * @brief 自动注册文件，系统自动分配地址
     * @param type 文件类型（EEFileType 枚举）
     * @param maxSize 该文件的最大数据大小（字节）
     * @return 注册是否成功
     *
     * 使用示例：
     *   EE.registerAuto(IIC_START, 1);      // I2C地址，1字节
     *   EE.registerAuto(KAL_MAN, 4);        // Kalman参数，4字节
     *   地址会自动分配：0x0A, 0x13, 0x1C 等
     */
    bool registerAuto(EEFileType type, uint16_t maxSize);

    // ========== 读写接口（使用枚举而非地址）==========
    /**
     * @brief 写入数据到 EEPROM
     * @param type 文件类型
     * @param data 待写入数据
     * @param length 数据长度
     * @return 写入是否成功
     */
    bool write(EEFileType type, const uint8_t* data, uint16_t length);

    /**
     * @brief 从 EEPROM 读取数据
     * @param type 文件类型
     * @param data 读取缓冲区
     * @param length 期望读取长度
     * @return 读取是否成功
     */
    bool read(EEFileType type, uint8_t* data, uint16_t length);

    // ========== 文件操作 ==========
    /**
     * @brief 清除指定文件
     */
    bool erase(EEFileType type);

    /**
     * @brief 设置文件启用/禁用状态
     */
    void setFileEnabled(EEFileType type, bool enabled);

    /**
     * @brief 获取文件启用状态
     */
    bool isFileEnabled(EEFileType type);

    /**
     * @brief 获取文件实际数据长度
     */
    uint16_t getFileDataLen(EEFileType type);

    /**
     * @brief 获取文件是否被修改
     */
    bool isFileModified(EEFileType type);

    /**
     * @brief 清除文件修改标志
     */
    void clearModifiedFlag(EEFileType type);

    /**
     * @brief 获取文件在 EEPROM 中的起始地址（用于调试）
     */
    uint16_t getFileAddr(EEFileType type);

    /**
     * @brief 获取文件有效标志位
     * @return true 表示数据有效，false 表示数据无效
     */
    bool isFileValid(EEFileType type);

    /**
     * @brief 设置文件有效标志位
     * @param valid true=有效, false=无效
     */
    void setFileValid(EEFileType type, bool valid);

    // ========== 调试接口 ==========
    void printStatus();
    void printFileInfo(EEFileType type);
};

extern HardwareSerial hwSerial;

// ============ 便捷接口宏（简化调用）============
#define EE EEFILE::getInstance()

// 一行初始化
#define EE_INIT() do { EE.begin(); EE.enable(); } while(0)

// 自动注册文件（推荐方式）
#define EE_REG(type, size) EE.registerAuto(type, size)

// 写入数据（使用枚举，地址自动对应）
#define EE_WRITE(type, data, len) EE.write(type, (uint8_t*)data, len)

// 读取数据
#define EE_READ(type, buffer, len) EE.read(type, buffer, len)

// 文件操作
#define EE_ERASE(type) EE.erase(type)
#define EE_ENABLE(type) EE.setFileEnabled(type, true)
#define EE_DISABLE(type) EE.setFileEnabled(type, false)

// 文件查询
#define EE_GET_LEN(type) EE.getFileDataLen(type)
#define EE_IS_MODIFIED(type) EE.isFileModified(type)
#define EE_CLEAR_MODIFIED(type) EE.clearModifiedFlag(type)
#define EE_GET_ADDR(type) EE.getFileAddr(type)

// 启动标志位（最重要的特性）
#define EE_IS_VALID(type) EE.isFileValid(type)       // 检查数据是否有效
#define EE_SET_VALID(type, v) EE.setFileValid(type, v)  // 设置有效标志

// 调试输出
#define EE_STATUS() EE.printStatus()
#define EE_INFO(type) EE.printFileInfo(type)

#endif