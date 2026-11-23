#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
#include <wmsdk.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "kernel/kernel.h"
#include "recomp_save_dir.h"
#include "xbox_soundtrack.h"

#define MUSIC_PATH_CAP 2048
#define DB_PREFIX ((1 + XST_MAX_SOUNDTRACKS) * XST_BLOCK)
#define DB_LIMIT (DB_PREFIX + XST_MAX_SOUNDTRACKS * XST_MAX_GROUPS * XST_BLOCK)
#define SOUNDTRACK_NAME_AT (16 + XST_MAX_GROUPS * 4)
#define TRACK_MIN_BYTES 131072

typedef HRESULT (WINAPI *CreateWriter)(IUnknown *, IWMWriter **);

enum music_result {
    MUSIC_IDLE, MUSIC_IMPORTING, MUSIC_ADDED, MUSIC_ADDED_PARTIAL,
    MUSIC_NO_TRACKS, MUSIC_DUPLICATE, MUSIC_BAD_DB, MUSIC_FULL, MUSIC_PATH,
    MUSIC_ENCODER, MUSIC_IO, MUSIC_PICKER
};
static volatile LONG music_result;
static volatile LONG music_restart_pending;

static int child_path(wchar_t out[MUSIC_PATH_CAP], const wchar_t *parent, const wchar_t *name)
{
    int n = swprintf(out, MUSIC_PATH_CAP, L"%ls\\%ls", parent, name);
    return n > 0 && n < MUSIC_PATH_CAP;
}

static int make_directory(const wchar_t *path)
{
    if (CreateDirectoryW(path, NULL))
        return 1;
    if (GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static uint32_t db_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void db_put_u32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (uint8_t)(v >> (8 * i));
}

/* Keep the original header, soundtrack blocks, and song groups. The Xbox
 * stores selection against the existing library order and soundtrack IDs. */
static enum music_result read_database(const wchar_t *path, uint8_t *db, DWORD *bytes)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND)
            return MUSIC_IO;
        *bytes = DB_PREFIX;
        db_put_u32(db, 1);
        db_put_u32(db + 8, 1);
        db_put_u32(db + 12 + XST_MAX_SOUNDTRACKS * 4, 0x10000);
        return MUSIC_ADDED;
    }
    DWORD size = GetFileSize(file, NULL);
    BOOL ok = size >= DB_PREFIX && size <= DB_LIMIT && size % XST_BLOCK == 0 &&
              ReadFile(file, db, size, bytes, NULL) && *bytes == size;
    CloseHandle(file);
    if (!ok || db_u32(db) != 1 || db_u32(db + 4) > XST_MAX_SOUNDTRACKS)
        return MUSIC_BAD_DB;
    return MUSIC_ADDED;
}

static int publish_database(const wchar_t *temporary, const wchar_t *path,
                            const uint8_t *db, DWORD bytes)
{
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    DWORD written;
    BOOL ok = WriteFile(file, db, bytes, &written, NULL) && written == bytes &&
              FlushFileBuffers(file);
    CloseHandle(file);
    if (ok && MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return 1;
    DeleteFileW(temporary);
    return 0;
}

static int track_path(wchar_t path[MUSIC_PATH_CAP], const wchar_t *dir, uint32_t id)
{
    wchar_t name[16];
    swprintf(name, 16, L"%08X.WMA", (unsigned)id);
    return child_path(path, dir, name);
}

static HRESULT write_track(CreateWriter create, const xst_song *song,
                           const wchar_t *target, uint32_t *duration)
{
    /* Windows Media Audio 8, 128 kbit/s stereo. Its default input is PCM
     * 16-bit stereo at 44.1 kHz, which the toolkit decoder produces. */
    static const GUID profile = {0x407b9450, 0x8bdc, 0x4ee5,
                                 {0x88, 0xb8, 0x6f, 0x52, 0x7b, 0xd9, 0x41, 0xf2}};
    xst_stream *stream = xst_stream_open(song->path);
    if (!stream)
        return E_FAIL;
    IWMWriter *writer = NULL;
    HRESULT hr = create(NULL, &writer);
    if (SUCCEEDED(hr)) hr = IWMWriter_SetProfileByID(writer, &profile);
    if (SUCCEEDED(hr)) hr = IWMWriter_SetOutputFilename(writer, target);
    if (SUCCEEDED(hr)) hr = IWMWriter_BeginWriting(writer);

    int16_t pcm[4096 * XST_CHANNELS];
    uint64_t frames = 0;
    size_t count;
    while (SUCCEEDED(hr) && (count = xst_stream_read(stream, pcm, 4096))) {
        INSSBuffer *sample = NULL;
        BYTE *data = NULL;
        DWORD bytes = (DWORD)(count * XST_CHANNELS * sizeof pcm[0]);
        hr = IWMWriter_AllocateSample(writer, bytes, &sample);
        if (SUCCEEDED(hr)) hr = INSSBuffer_GetBuffer(sample, &data);
        if (SUCCEEDED(hr)) {
            memcpy(data, pcm, bytes);
            hr = INSSBuffer_SetLength(sample, bytes);
        }
        if (SUCCEEDED(hr))
            hr = IWMWriter_WriteSample(writer, 0, frames * 10000000 / XST_RATE, 0, sample);
        if (sample)
            INSSBuffer_Release(sample);
        frames += count;
    }
    if (SUCCEEDED(hr)) hr = IWMWriter_EndWriting(writer);
    if (writer)
        IWMWriter_Release(writer);
    xst_stream_close(stream);
    if (FAILED(hr) || !frames)
        return FAILED(hr) ? hr : E_FAIL;

    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (!GetFileAttributesExW(target, GetFileExInfoStandard, &attributes))
        return HRESULT_FROM_WIN32(GetLastError());
    if (!attributes.nFileSizeHigh && attributes.nFileSizeLow < TRACK_MIN_BYTES)
        return S_FALSE;
    *duration = (uint32_t)(frames * 1000 / XST_RATE);
    return S_OK;
}

static int append_soundtrack(uint8_t *db, DWORD *bytes, xst_library *library, uint32_t id)
{
    unsigned index = db_u32(db + 4);
    unsigned first_group = (*bytes - DB_PREFIX) / XST_BLOCK;
    size_t size = xst_db_size(library);
    uint8_t *add = malloc(size);
    if (!add)
        return 0;

    xst_soundtrack *st = &library->st[0];
    st->id = id;
    for (int i = 0; i < st->count; ++i)
        library->songs[i].id = (id << 16) | (unsigned)i;
    if (xst_db_build(library, add, size) != size || size - DB_PREFIX > DB_LIMIT - *bytes) {
        free(add);
        return 0;
    }
    uint8_t *row = db + (index + 1) * XST_BLOCK;
    memcpy(row, add + XST_BLOCK, XST_BLOCK);
    memcpy(db + *bytes, add + DB_PREFIX, size - DB_PREFIX);
    for (unsigned i = 0; i < (size - DB_PREFIX) / XST_BLOCK; ++i) {
        db_put_u32(row + 12 + i * 4, first_group + i);
        db_put_u32(db + *bytes + i * XST_BLOCK + 8, first_group + i);
    }
    *bytes += (DWORD)(size - DB_PREFIX);
    db_put_u32(db + 12 + index * 4, id);
    db_put_u32(db + 4, index + 1);
    db_put_u32(db + 8, id + 1);
    db_put_u32(db + 12 + XST_MAX_SOUNDTRACKS * 4, (id + 1) << 16);
    free(add);
    return 1;
}

static enum music_result import_folder(const wchar_t *folder, const wchar_t *save_root)
{
    char utf8[4096];
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, folder, -1,
                             utf8, sizeof utf8, NULL, NULL))
        return MUSIC_PATH;
    xst_library *library = xst_scan_single(utf8);
    if (!library || !library->n_songs) {
        xst_free(library);
        return MUSIC_NO_TRACKS;
    }

    wchar_t title_data[MUSIC_PATH_CAP], system_title[MUSIC_PATH_CAP], music[MUSIC_PATH_CAP];
    wchar_t database[MUSIC_PATH_CAP], temporary[MUSIC_PATH_CAP], staging[MUSIC_PATH_CAP];
    wchar_t album[MUSIC_PATH_CAP], track[MUSIC_PATH_CAP], name[64];
    wchar_t token[40];
    GUID guid;
    uint8_t *db = NULL;
    HMODULE module = NULL;
    DWORD bytes = 0;
    unsigned written = 0;
    unsigned id = 0;
    int has_stage = 0, published_album = 0;
    enum music_result result = MUSIC_IO;

    if (!child_path(title_data, save_root, L"TDATA") ||
        !child_path(system_title, title_data, L"FFFE0000") ||
        !child_path(music, system_title, L"MUSIC") ||
        !child_path(database, music, L"ST.DB") ||
        !make_directory(title_data) || !make_directory(system_title) || !make_directory(music))
        goto finish;
    db = calloc(1, DB_LIMIT);
    if (!db)
        goto finish;
    result = read_database(database, db, &bytes);
    if (result != MUSIC_ADDED)
        goto finish;
    unsigned rows = db_u32(db + 4);
    id = db_u32(db + 8);
    if (!id) id = 1;
    result = MUSIC_FULL;
    if (rows == XST_MAX_SOUNDTRACKS)
        goto finish;
    for (unsigned i = 0; i < rows; ++i) {
        uint32_t old = db_u32(db + 12 + i * 4);
        const uint8_t *row = db + (i + 1) * XST_BLOCK;
        if (!old || old > 0xfffe || db_u32(row) != 0x21371 || db_u32(row + 4) != old) {
            result = MUSIC_BAD_DB;
            goto finish;
        }
        if (id <= old) id = old + 1;
        if (CompareStringOrdinal((const wchar_t *)(row + SOUNDTRACK_NAME_AT), XST_NAME_UNITS,
                                 (const wchar_t *)library->st[0].name, XST_NAME_UNITS,
                                 TRUE) == CSTR_EQUAL) {
            result = MUSIC_DUPLICATE;
            goto finish;
        }
    }
    /* A previous interruption can leave an unreferenced album directory.
     * Never reuse or overwrite it when assigning the next ID. */
    for (; id <= 0xfffe; ++id) {
        swprintf(name, 64, L"%04X", id);
        if (!child_path(album, music, name)) goto finish;
        if (GetFileAttributesW(album) == INVALID_FILE_ATTRIBUTES &&
            GetLastError() == ERROR_FILE_NOT_FOUND)
            break;
    }
    if (id > 0xfffe)
        goto finish;

    result = MUSIC_ENCODER;
    module = LoadLibraryExW(L"wmvcore.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    CreateWriter create = module ? (CreateWriter)GetProcAddress(module, "WMCreateWriter") : NULL;
    if (!create)
        goto finish;
    result = MUSIC_IO;
    if (FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(&guid, token, 40))
        goto finish;
    swprintf(name, 64, L".ampedup-%ls", token);
    if (!child_path(staging, music, name) || !CreateDirectoryW(staging, NULL))
        goto finish;
    has_stage = 1;
    swprintf(name, 64, L".ampedup-%ls.db", token);
    if (!child_path(temporary, music, name))
        goto finish;

    xst_song *accepted = calloc(library->n_songs, sizeof *accepted);
    if (!accepted)
        goto finish;
    uint32_t duration = 0;
    for (int i = 0; i < library->n_songs; ++i) {
        uint32_t song_id = (id << 16) | written;
        if (!track_path(track, staging, song_id)) {
            free(accepted);
            goto finish;
        }
        xst_song song = library->songs[i];
        HRESULT hr = write_track(create, &song, track, &song.ms);
        if (hr != S_OK) {
            DeleteFileW(track);
            if (hr == S_FALSE)
                continue;
            (fprintf)(stderr, "[AMPEDUP MUSIC] Encoding failed (0x%08lx)\n", (unsigned long)hr);
            result = MUSIC_ENCODER;
            free(accepted);
            goto finish;
        }
        accepted[written++] = song;
        duration += song.ms;
    }
    if (!written) {
        result = MUSIC_NO_TRACKS;
        free(accepted);
        goto finish;
    }
    const int partial = written != (unsigned)library->n_songs || library->bad_files ||
                        library->dropped_songs || library->budget_hit;
    xst_library one = {0};
    one.n_st = 1;
    one.st[0] = library->st[0];
    one.st[0].count = written;
    one.st[0].ms = duration;
    one.songs = accepted;
    const int appended = append_soundtrack(db, &bytes, &one, id);
    free(accepted);
    if (!appended || !MoveFileExW(staging, album, MOVEFILE_WRITE_THROUGH))
        goto finish;
    has_stage = 0;
    published_album = 1;
    if (!publish_database(temporary, database, db, bytes))
        goto finish;
    published_album = 0;
    result = partial ? MUSIC_ADDED_PARTIAL : MUSIC_ADDED;
    (fprintf)(stderr, "[AMPEDUP MUSIC] Added genre %u with %u tracks\n", id, written);

finish:
    if (has_stage || published_album) {
        const wchar_t *directory = has_stage ? staging : album;
        for (unsigned i = 0; i < written; ++i)
            if (track_path(track, directory, (id << 16) | i))
                DeleteFileW(track);
        RemoveDirectoryW(directory);
    }
    if (module)
        FreeLibrary(module);
    free(db);
    xst_free(library);
    return result;
}

static DWORD WINAPI music_worker(void *chosen)
{
    PWSTR folder = chosen;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    enum music_result result = MUSIC_ENCODER;
    if (SUCCEEDED(hr)) {
        wchar_t root[MUSIC_PATH_CAP];
        const char *save_root = recomp_save_dir("ampedup");
        result = save_root && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, save_root, -1,
                                                  root, MUSIC_PATH_CAP)
                 ? import_folder(folder, root) : MUSIC_PATH;
        CoUninitialize();
    }
    CoTaskMemFree(folder);
    InterlockedExchange(&music_result, result);
    if (result == MUSIC_ADDED || result == MUSIC_ADDED_PARTIAL)
        InterlockedExchange(&music_restart_pending, 1);
    return 0;
}

struct folder_entry_text {
    const char *hint;
    const wchar_t *title;
};

/* Use the Xbox language that selected the guest menu. Keep the native picker
 * and the UTF-8 overlay on the same wording. */
#define FOLDER_TEXT(label) { u8"Y   " u8 ## label, L"AMPEDUP! - " L ## label }
static const struct folder_entry_text folder_text[] = {
    [1] = FOLDER_TEXT("Add Music Folder"),
    [3] = FOLDER_TEXT("Musikordner hinzuf\u00fcgen"),
    [4] = FOLDER_TEXT("Ajouter un dossier musical"),
    [5] = FOLDER_TEXT("A\u00f1adir carpeta de m\u00fasica"),
    [6] = FOLDER_TEXT("Aggiungi cartella musicale")
};
#undef FOLDER_TEXT

static const struct folder_entry_text *music_folder_text(void)
{
    ULONG language = xbox_GetLanguage();
    if (language >= ARRAYSIZE(folder_text) || !folder_text[language].hint)
        language = XC_LANGUAGE_ENGLISH;
    return &folder_text[language];
}

const char *amped_music_folder_hint(void)
{
    return music_folder_text()->hint;
}

int amped_music_take_restart_prompt(void)
{
    return InterlockedExchange(&music_restart_pending, 0) != 0;
}

/* Called on the window thread in response to the Soundtracks action. */
void amped_music_pick_folder(void *owner)
{
    if (InterlockedCompareExchange(&music_result, MUSIC_IDLE, MUSIC_IDLE) == MUSIC_IMPORTING)
        return;
    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT hr = init;
    IFileOpenDialog *dialog = NULL;
    PWSTR folder = NULL;
    if (SUCCEEDED(hr))
        hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                              &IID_IFileOpenDialog, (void **)&dialog);
    if (SUCCEEDED(hr))
        hr = IFileOpenDialog_SetTitle(dialog, music_folder_text()->title);
    if (SUCCEEDED(hr))
        hr = IFileOpenDialog_SetOptions(dialog, FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                                        FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    if (SUCCEEDED(hr))
        hr = IFileOpenDialog_Show(dialog, (HWND)owner);
    if (SUCCEEDED(hr)) {
        IShellItem *item = NULL;
        hr = IFileOpenDialog_GetResult(dialog, &item);
        if (SUCCEEDED(hr)) {
            hr = IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &folder);
            IShellItem_Release(item);
        }
    }
    if (dialog)
        IFileOpenDialog_Release(dialog);
    if (SUCCEEDED(init))
        CoUninitialize();
    if (SUCCEEDED(hr) && folder) {
        InterlockedExchange(&music_result, MUSIC_IMPORTING);
        HANDLE thread = CreateThread(NULL, 0, music_worker, folder, 0, NULL);
        if (thread) {
            CloseHandle(thread);
            return;
        }
        CoTaskMemFree(folder);
        InterlockedExchange(&music_result, MUSIC_IO);
    } else if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        InterlockedExchange(&music_result, MUSIC_PICKER);
    }
}

const char *amped_music_status(void)
{
    switch (InterlockedCompareExchange(&music_result, MUSIC_IDLE, MUSIC_IDLE)) {
    case MUSIC_IMPORTING: return "Importing music...";
    case MUSIC_ADDED: return "Genre ready for the next launch.";
    case MUSIC_ADDED_PARTIAL: return "Genre ready for the next launch. Some tracks were skipped.";
    case MUSIC_NO_TRACKS: return "No supported music long enough for Amped was found.";
    case MUSIC_DUPLICATE: return "A genre with this folder name already exists.";
    case MUSIC_BAD_DB: return "The existing soundtrack library could not be read.";
    case MUSIC_FULL: return "The soundtrack library is full.";
    case MUSIC_PATH: return "This music or profile path could not be opened.";
    case MUSIC_ENCODER: return "Windows could not encode a track as WMA.";
    case MUSIC_IO: return "The soundtrack could not be saved.";
    case MUSIC_PICKER: return "Windows could not open the music folder picker.";
    default: return NULL;
    }
}
