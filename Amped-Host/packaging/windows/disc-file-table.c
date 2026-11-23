/* Report the game partition's file extents. The installer supplies all output paths. */
#include "xdvdfs.h"
#include <inttypes.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    xdvdfs_image *image;
    char error[512];
    if (argc != 2)
        return 1;
    if (xdvdfs_open(argv[1], &image, error, sizeof(error)) != XDVDFS_OK) {
        puts(error);
        return 1;
    }
    const xdvdfs_file *files = xdvdfs_files(image);
    for (unsigned i = 0; i < xdvdfs_count(image); ++i) {
        /* The reader rejects tabs and newlines in disc names. Keep retail filename bytes. */
        printf("%" PRIu64 "\t%" PRIu32 "\t%s\n", files[i].offset, files[i].size, files[i].path);
    }
    xdvdfs_close(image);
    return ferror(stdout) ? 1 : 0;
}
