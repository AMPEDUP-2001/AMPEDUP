#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* Match only the exact observed source DVD path; return 0 for unrelated paths,
 * 1 for a valid replacement, -1 to fail closed for unsafe/missing override. */
int amped_demo_asset_redirect(const char *path,uint32_t access,uint32_t disposition,uint32_t options,wchar_t *out,unsigned cap) {
 const uint32_t denied=0x40000000u|0x10000000u|0x00000002u|0x00000004u|0x00000100u|0x00010000u|0x00040000u|0x00080000u;
 const char *value; int count;
 if (!path) return 0;
 if (_strnicmp(path,"\\??\\",4)==0) path+=4;
 if (_stricmp(path,"d:\\resource\\shell\\Menus.res")!=0 && _stricmp(path,"\\Device\\CdRom0\\resource\\shell\\Menus.res")!=0) return 0;
 if ((access & denied)!=0 || disposition!=1u || (options & 1u)!=0) return -1;
 value=getenv("AMPED_DEMO_ASSET_OVERRIDE");
 if (!value || !*value || !out || cap==0) return -1;
 count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,-1,out,(int)cap);
 if (count==0 || GetFileAttributesW(out)==INVALID_FILE_ATTRIBUTES || (GetFileAttributesW(out)&FILE_ATTRIBUTE_DIRECTORY)) return -1;
 if (getenv("AMPED_DEMO_NOTICE_TRACE")) (fprintf)(stderr,"[AMPED_DEMO_ASSET] exact DVD read redirected to isolated derivative\n");
 return 1;
}
