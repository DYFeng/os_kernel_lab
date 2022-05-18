#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

/**
 * @brief 把文件做成一个512-byte的引导分区
 * @param argc
 * @param argv
 * @return
 */
int
main(int argc, char *argv[]) {
    struct stat st;
    // 检查参数个数
    if (argc != 3) {
        fprintf(stderr, "Usage: <input filename> <output filename>\n");
        return -1;
    }
    // 尝试获取文件状态（大小）
    if (stat(argv[1], &st) != 0) {
        fprintf(stderr, "Error opening file '%s': %s\n", argv[1], strerror(errno));
        return -1;
    }
    printf("'%s' size: %lld bytes\n", argv[1], (long long)st.st_size);
    // 因为一个扇区为512byte，后两个byte还是签名，所以可用的大小只有510，所以原文件大小不能超过510
    if (st.st_size > 510) {
        fprintf(stderr, "%lld >> 510!!\n", (long long)st.st_size);
        return -1;
    }
    // 创建一个512byte大小的内存buf并置0
    char buf[512];
    memset(buf, 0, sizeof(buf));
    // 把文件内容拷贝到buf
    FILE *ifp = fopen(argv[1], "rb");
    int size = fread(buf, 1, st.st_size, ifp);
    if (size != st.st_size) {
        fprintf(stderr, "read '%s' error, size is %d.\n", argv[1], size);
        return -1;
    }
    fclose(ifp);
    // 把buf的最后两byte置 0x55 0xAA，以标记是一个引导扇区
    buf[510] = 0x55;
    buf[511] = 0xAA;
    // 把buf写入到输出文件
    FILE *ofp = fopen(argv[2], "wb+");
    size = fwrite(buf, 1, 512, ofp);
    if (size != 512) {
        fprintf(stderr, "write '%s' error, size is %d.\n", argv[2], size);
        return -1;
    }
    fclose(ofp);
    printf("build 512 bytes boot sector: '%s' success!\n", argv[2]);
    return 0;
}

