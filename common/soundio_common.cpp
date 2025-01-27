// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection
#include "audio.h"
#include "auduncmp.h"
#include "endianness.h"
#include "file.h"
#include "memflag.h"
#include "soscomp.h"
#include "sound.h"
#include "soundio_imp.h"
#include <algorithm>
#include <stdlib.h>

enum
{
    AUD_CHUNK_MAGIC_ID = 0x0000DEAF,
    VOLUME_MIN = 0,
    VOLUME_MAX = 255,
    PRIORITY_MIN = 0,
    PRIORITY_MAX = 255,
    MAX_SAMPLE_TRACKERS = 5, // C&C issue where sounds get cut off is because of the small number of trackers.
    STREAM_BUFFER_COUNT = 16,
    BUFFER_CHUNK_SIZE = 8192, // 256 * 32,
    UNCOMP_BUFFER_SIZE = 2098,
    BUFFER_TOTAL_BYTES = BUFFER_CHUNK_SIZE * 4, // 32 kb
    TIMER_DELAY = 25,
    TIMER_RESOLUTION = 1,
    TIMER_TARGET_RESOLUTION = 10, // 10-millisecond target resolution
    INVALID_AUDIO_HANDLE = -1,
    INVALID_FILE_HANDLE = -1,
};

/*
** Define the different type of sound compression avaliable to the westwood
** library.
*/
typedef enum
{
    SCOMP_NONE = 0,     // No compression -- raw data.
    SCOMP_WESTWOOD = 1, // Special sliding window delta compression.
    SCOMP_SOS = 99      // SOS frame compression.
} SCompressType;

struct SampleTrackerType
{
    /*
    **	This flags whether this sample structure is active or not.
    */
    bool Active;

    /*
    **	This flags whether the sample is loading or has been started.
    */
    bool Loading;

    /*
    **	If this sample is really to be considered a score rather than
    **	a sound effect, then special rules apply.  These largely fall into
    **	the area of volume control.
    */
    bool IsScore;

    /*
    **	This is the original sample pointer. It is used to control the sample based on
    **	pointer rather than handle. The handle method is necessary when more than one
    **	sample could be playing simultaneously. The pointer method is necessary when
    **	the dealing with a sample that may have stopped behind the programmer's back and
    **	this occurance is not otherwise determinable.  It is also used in
    ** conjunction with original size to unlock a sample which has been DPMI
    ** locked.
    */
    const void* Original;
    int OriginalSize;

    /*
    ** Variable to keep track of the playback rate of this buffer
    */
    int Frequency;

    /*
    ** Variable to keep track of the sample type ( 8 or 16 bit ) of this buffer
    */
    int BitsPerSample;

    /*
    ** Variable to keep track of the stereo ability of this buffer
    */
    bool Stereo;

    /*
    **	Samples maintain a priority which is used to determine
    **	which sounds live or die when the maximum number of
    **	sounds are being played.
    */
    int Priority;

    /*
    **	This is the current volume of the sample as it is being played.
    */
    int Volume;
    int Reducer; // Amount to reduce volume per tick.

    /*
    **	This is the compression that the sound data is using.
    */
    SCompressType Compression;

    /*
    **	This flag indicates whether this sample needs servicing.
    **	Servicing entails filling one of the empty low buffers.
    */
    short Service;

    /*
    **	This flag is true when the sample has stopped playing,
    **	BUT there is more data available.  The sample must be
    **	restarted upon filling the low buffer.
    */
    bool Restart;

    /*
    **	Streaming control handlers.
    */
    bool (*Callback)(short id, short* odd, void** buffer, int* size);
    int FilePending;     // Number of buffers already filled ahead.
    int FilePendingSize; // Number of bytes in last filled buffer.
    short Odd;           // Block number tracker (0..StreamBufferCount-1).
    void* QueueBuffer;   // Pointer to continued sample data.
    int QueueSize;       // Size of queue buffer attached.

    /*
    **	The file variables are used when streaming directly off of the
    **	hard drive.
    */
    int FileHandle; // Streaming file handle (INVALID_FILE_HANDLE = not in use).
    void* FileBuffer;

    /*
    ** The following structure is used if the sample if compressed using
    ** the sos 16 bit compression Codec.
    */
    _SOS_COMPRESS_INFO sosinfo;

    /*
    **	This flag indicates that there is more source data
    **  to copy to the play buffer
    **
    */
    bool MoreSource;

    /*
    **	This flag indicates that the entire sample fitted inside the
    ** direct sound secondary buffer
    **
    */
    bool OneShot;

    /*
    **	Pointer to the sound data that has not yet been copied
    **	to the playback buffers.
    */
    void* Source;

    /*
    **	This is the number of bytes remaining in the source data as
    **	pointed to by the "Source" element.
    */
    int Remainder;

    struct SampleTrackerTypeImp* Imp;
};

struct LockedDataType
{
    unsigned int DigiHandle; // = -1;
    bool ServiceSomething;   // = false;
    unsigned MagicNumber;    // = 0xDEAF;
    void* UncompBuffer;      // = NULL;
    int StreamBufferSize;    // = (2*SECONDARY_BUFFER_SIZE)+128;
    short StreamBufferCount; // = 32;
    SampleTrackerType SampleTracker[MAX_SAMPLE_TRACKERS];
    unsigned SoundVolume;
    unsigned ScoreVolume;
    int VolumeLock;
};

void (*Audio_Focus_Loss_Function)() = nullptr;

static struct LockedDataType LockedData;
SFX_Type SoundType;
Sample_Type SampleType;
static void* FileStreamBuffer = nullptr;
bool StreamLowImpact = false;
static bool StartingFileStream = false;
static bool volatile AudioDone = false;
extern bool GameInFocus;
static uint8_t ChunkBuffer[BUFFER_CHUNK_SIZE];

bool Any_Locked(); // From each games winstub.cpp at the moment.
static int Get_Free_Sample_Handle(int priority);
static void Maintenance_Callback();
static int Play_Sample_Handle(const void* sample, int priority, int volume, signed short panloc, int id);
static int Sample_Read(int fh, void* buffer, int size);

static void Init_Locked_Data()
{
    LockedData.DigiHandle = INVALID_AUDIO_HANDLE;
    LockedData.ServiceSomething = false;
    LockedData.MagicNumber = AUD_CHUNK_MAGIC_ID;
    LockedData.UncompBuffer = 0;
    LockedData.StreamBufferSize = BUFFER_CHUNK_SIZE + 128;
    LockedData.StreamBufferCount = STREAM_BUFFER_COUNT;
    LockedData.SoundVolume = VOLUME_MAX;
    LockedData.ScoreVolume = VOLUME_MAX;
}

static int Simple_Copy(void** source, int* ssize, void** alternate, int* altsize, void** dest, int size)
{
    int out = 0;

    if (*ssize == 0) {
        *source = *alternate;
        *ssize = *altsize;
        *alternate = nullptr;
        *altsize = 0;
    }

    if (*source == nullptr || *ssize == 0) {
        return out;
    }

    int s = size;

    if (*ssize < size) {
        s = *ssize;
    }

    memcpy(*dest, *source, s);
    *source = static_cast<char*>(*source) + s;
    *ssize -= s;
    *dest = static_cast<char*>(*dest) + s;
    out = s;

    if ((size - s) == 0) {
        return out;
    }

    *source = *alternate;
    *ssize = *altsize;
    *alternate = nullptr;
    *altsize = 0;

    out = Simple_Copy(source, ssize, alternate, altsize, dest, (size - s)) + s;

    return out;
}

static int Sample_Copy(SampleTrackerType* st,
                       void** source,
                       int* ssize,
                       void** alternate,
                       int* altsize,
                       void* dest,
                       int size,
                       SCompressType scomp,
                       void* trailer,
                       int16_t* trailersize)
{
    int datasize = 0;

    // There is no compression or it doesn't match any of the supported compressions so we just copy the data over.
    if (scomp == SCOMP_NONE || (scomp != SCOMP_WESTWOOD && scomp != SCOMP_SOS)) {
        return Simple_Copy(source, ssize, alternate, altsize, &dest, size);
    }

    _SOS_COMPRESS_INFO* s = &st->sosinfo;

    while (size > 0) {
        uint16_t fsize;
        uint16_t dsize;
        unsigned magic;

        void* fptr = &fsize;
        void* dptr = &dsize;
        void* mptr = &magic;

        // Verify and seek over the chunk header.
        if (Simple_Copy(source, ssize, alternate, altsize, &fptr, sizeof(fsize)) < sizeof(fsize)) {
            break;
        }

        fsize = le16toh(fsize);

        if (Simple_Copy(source, ssize, alternate, altsize, &dptr, sizeof(dsize)) < sizeof(dsize)) {
            break;
        }

        dsize = le16toh(dsize);

        if (dsize > size) {
            break;
        }

        if (Simple_Copy(source, ssize, alternate, altsize, &mptr, sizeof(magic)) < sizeof(magic)) {
            break;
        }

        magic = le32toh(magic);

        if (magic != LockedData.MagicNumber) {
            break;
        }

        if (fsize == dsize) {
            // File size matches size to decompress, so there's nothing to do other than copy the buffer over.
            if (Simple_Copy(source, ssize, alternate, altsize, &dest, fsize) < dsize) {
                return datasize;
            }
        } else {
            // Else we need to decompress it.
            void* uptr = LockedData.UncompBuffer;

            if (Simple_Copy(source, ssize, alternate, altsize, &uptr, fsize) < fsize) {
                return datasize;
            }

            if (scomp == SCOMP_WESTWOOD) {
                Audio_Unzap(LockedData.UncompBuffer, dest, dsize);
            } else {
                s->lpSource = (char*)LockedData.UncompBuffer;
                s->lpDest = (char*)dest;

                sosCODECDecompressData(s, dsize);
            }

            dest = reinterpret_cast<char*>(dest) + dsize;
        }

        datasize += dsize;
        size -= dsize;
    }

    return datasize;
}

static int
Stream_Sample_Vol(void* buffer, int size, bool (*callback)(short, short*, void**, int*), int volume, int handle)
{
    if (AudioDone || buffer == nullptr || size == 0 || LockedData.DigiHandle == INVALID_AUDIO_HANDLE) {
        return INVALID_AUDIO_HANDLE;
    }

    AUDHeaderType header;
    memcpy(&header, buffer, sizeof(header));
    int oldsize = header.Size;
    header.Size = htole32(size - sizeof(header));
    memcpy(buffer, &header, sizeof(header));
    int playid = Play_Sample_Handle(buffer, PRIORITY_MAX, volume, 0, handle);
    header.Size = oldsize;
    memcpy(buffer, &header, sizeof(header));

    if (playid == INVALID_AUDIO_HANDLE) {
        return INVALID_AUDIO_HANDLE;
    }

    SampleTrackerType* st = &LockedData.SampleTracker[playid];
    st->Callback = callback;
    st->Odd = 0;

    return playid;
}

static bool File_Callback(short id, short* odd, void** buffer, int* size)
{
    if (id == INVALID_AUDIO_HANDLE) {
        return false;
    }

    SampleTrackerType* st = &LockedData.SampleTracker[id];

    if (st->FileBuffer == nullptr) {
        return false;
    }

    if (*buffer == nullptr && st->FilePending) {
        *buffer =
            static_cast<char*>(st->FileBuffer) + LockedData.StreamBufferSize * (*odd % LockedData.StreamBufferCount);
        --st->FilePending;
        ++*odd;
        *size = st->FilePending == 0 ? st->FilePendingSize : LockedData.StreamBufferSize;
    }

    Maintenance_Callback();

    int count = StreamLowImpact ? LockedData.StreamBufferCount / 2 : LockedData.StreamBufferCount - 3;

    if (count > st->FilePending && st->FileHandle != INVALID_FILE_HANDLE) {
        if (LockedData.StreamBufferCount - 2 != st->FilePending) {
            // Fill empty buffers.
            for (int num_empty_buffers = LockedData.StreamBufferCount - 2 - st->FilePending;
                 num_empty_buffers && st->FileHandle != INVALID_FILE_HANDLE;
                 --num_empty_buffers) {
                // Buffer to fill with data.
                void* tofill =
                    static_cast<char*>(st->FileBuffer)
                    + LockedData.StreamBufferSize * ((st->FilePending + *odd) % LockedData.StreamBufferCount);

                int psize = Read_File(st->FileHandle, tofill, LockedData.StreamBufferSize);

                if (psize != LockedData.StreamBufferSize) {
                    Close_File(st->FileHandle);
                    st->FileHandle = INVALID_FILE_HANDLE;
                }

                if (psize > 0) {
                    st->FilePendingSize = psize;
                    ++st->FilePending;
                    Maintenance_Callback();
                }
            }
        }

        if (st->QueueBuffer == nullptr && st->FilePending) {
            st->QueueBuffer = static_cast<char*>(st->FileBuffer)
                              + LockedData.StreamBufferSize * (st->Odd % LockedData.StreamBufferCount);
            --st->FilePending;
            ++st->Odd;
            st->QueueSize = st->FilePending > 0 ? LockedData.StreamBufferSize : st->FilePendingSize;
        }

        Maintenance_Callback();
    }

    if (st->FilePending) {
        return true;
    }

    return false;
}

static void File_Stream_Preload(int index)
{
    SampleTrackerType* st = &LockedData.SampleTracker[index];
    int maxnum = (LockedData.StreamBufferCount / 2) + 4;
    int num = st->Loading ? std::min<int>(st->FilePending + 2, maxnum) : maxnum;

    int i = 0;

    for (i = st->FilePending; i < num; ++i) {
        int size = Read_File(st->FileHandle,
                             static_cast<char*>(st->FileBuffer) + i * LockedData.StreamBufferSize,
                             LockedData.StreamBufferSize);

        if (size > 0) {
            st->FilePendingSize = size;
            ++st->FilePending;
        }

        if (size < LockedData.StreamBufferSize) {
            break;
        }
    }

    Maintenance_Callback();

    if (LockedData.StreamBufferSize > st->FilePendingSize || i == maxnum) {
        int old_vol = LockedData.SoundVolume;

        int stream_size = st->FilePending == 1 ? st->FilePendingSize : LockedData.StreamBufferSize;

        LockedData.SoundVolume = LockedData.ScoreVolume;
        StartingFileStream = true;
        Stream_Sample_Vol(st->FileBuffer, stream_size, File_Callback, st->Volume, index);
        StartingFileStream = false;

        LockedData.SoundVolume = old_vol;

        st->Loading = false;
        --st->FilePending;

        if (st->FilePending == 0) {
            st->Odd = 0;
            st->QueueBuffer = 0;
            st->QueueSize = 0;
            st->FilePendingSize = 0;
            st->Callback = nullptr;
            Close_File(st->FileHandle);
        } else {
            st->Odd = 2;
            --st->FilePending;

            if (st->FilePendingSize != LockedData.StreamBufferSize) {
                Close_File(st->FileHandle);
                st->FileHandle = INVALID_FILE_HANDLE;
            }

            st->QueueBuffer = static_cast<char*>(st->FileBuffer) + LockedData.StreamBufferSize;
            st->QueueSize = st->FilePending == 0 ? st->FilePendingSize : LockedData.StreamBufferSize;
        }
    }
}

int File_Stream_Sample_Vol(char const* filename, int volume, bool real_time_start)
{
    if (LockedData.DigiHandle == INVALID_AUDIO_HANDLE || filename == nullptr || !Find_File(filename)) {
        return INVALID_AUDIO_HANDLE;
    }

    if (FileStreamBuffer == nullptr) {
        FileStreamBuffer = malloc((unsigned int)(LockedData.StreamBufferSize * LockedData.StreamBufferCount));

        for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
            LockedData.SampleTracker[i].FileBuffer = FileStreamBuffer;
        }
    }

    if (FileStreamBuffer == nullptr) {
        return INVALID_AUDIO_HANDLE;
    }

    int fh = Open_File(filename, 1);

    if (fh == INVALID_FILE_HANDLE) {
        return INVALID_AUDIO_HANDLE;
    }

    int handle = Get_Free_Sample_Handle(PRIORITY_MAX);

    if (handle < MAX_SAMPLE_TRACKERS) {
        SampleTrackerType* st = &LockedData.SampleTracker[handle];
        st->IsScore = true;
        st->FilePending = 0;
        st->FilePendingSize = 0;
        st->Loading = real_time_start;
        st->Volume = volume;
        st->FileHandle = fh;
        File_Stream_Preload(handle);
        return handle;
    }

    return INVALID_AUDIO_HANDLE;
}

void Sound_Callback()
{
    if (!AudioDone && LockedData.DigiHandle != INVALID_AUDIO_HANDLE) {
        Maintenance_Callback();

        for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
            SampleTrackerType* st = &LockedData.SampleTracker[i];

            // Is a load pending?
            if (st->Loading) {
                File_Stream_Preload(i);
                // We are done with this sample.
                continue;
            }

            // Is this sample inactive?
            if (!st->Active) {
                // If so, we close the handle.
                if (st->FileHandle != INVALID_FILE_HANDLE) {
                    Close_File(st->FileHandle);
                    st->FileHandle = INVALID_FILE_HANDLE;
                }
                // We are done with this sample.
                continue;
            }

            // Has it been faded Is the volume 0?
            if (st->Reducer && !st->Volume) {
                // If so stop it.
                Stop_Sample(i);

                // We are done with this sample.
                continue;
            }

            // Process pending files.
            if (st->QueueBuffer == nullptr
                || st->FileHandle != INVALID_FILE_HANDLE && LockedData.StreamBufferCount - 3 > st->FilePending) {
                if (st->Callback != nullptr) {
                    if (!st->Callback(i, &st->Odd, &st->QueueBuffer, &st->QueueSize)) {
                        // No files are pending so pending file callback not needed anymore.
                        st->Callback = nullptr;
                    }
                }

                // We are done with this sample.
                continue;
            }
        }
    }
}

static void Maintenance_Callback()
{
    if (AudioDone) {
        return;
    }

    SampleTrackerType* st = LockedData.SampleTracker;

    for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
        if (st->Active) { // If this tracker needs processing and isn't already marked as being processed, then process it.
            if (st->Service) {
                // Do we have more data in this tracker to play?
                if (st->MoreSource) {
                    int processed_buffers;

                    // Work out if we have any space to buffer more data right now.
                    processed_buffers = SoundImp_Get_Sample_Free_Buffer_Count(st->Imp);

                    while (processed_buffers > 0 && st->MoreSource) {
                        int bytes_copied = Sample_Copy(st,
                                                       &st->Source,
                                                       &st->Remainder,
                                                       &st->QueueBuffer,
                                                       &st->QueueSize,
                                                       ChunkBuffer,
                                                       BUFFER_CHUNK_SIZE,
                                                       st->Compression,
                                                       nullptr,
                                                       nullptr);

                        if (bytes_copied != BUFFER_CHUNK_SIZE) {
                            st->MoreSource = false;
                        }

                        if (bytes_copied > 0) {
                            SoundImp_Buffer_Sample_Data(st->Imp, ChunkBuffer, bytes_copied);
                            --processed_buffers;
                        }
                    }
                } else {
                    if (!SoundImp_Sample_Status(st->Imp)) {
                        st->Service = 0;
                        Stop_Sample(i);
                    }
                }
            }

            if (!st->QueueBuffer && st->FilePending != 0) {
                st->QueueBuffer = static_cast<char*>(st->FileBuffer)
                                  + LockedData.StreamBufferSize * (st->Odd % LockedData.StreamBufferCount);
                --st->FilePending;
                ++st->Odd;

                if (st->FilePending != 0) {
                    st->QueueSize = LockedData.StreamBufferSize;
                } else {
                    st->QueueSize = st->FilePendingSize;
                }
            }
        }

        ++st;
    }

    // Perform any volume modifications that need to be made.
    if (LockedData.VolumeLock == 0) {
        ++LockedData.VolumeLock;
        st = LockedData.SampleTracker;

        for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
            if (st->Active && st->Reducer > 0 && st->Volume > 0) {
                if (st->Reducer >= st->Volume) {
                    st->Volume = VOLUME_MIN;
                } else {
                    st->Volume -= st->Reducer;
                }

                if (!st->IsScore) {
                    SoundImp_Set_Sample_Volume(st->Imp, LockedData.SoundVolume * st->Volume);
                } else {
                    SoundImp_Set_Sample_Volume(st->Imp, LockedData.ScoreVolume * st->Volume);
                }
            }

            ++st;
        }

        --LockedData.VolumeLock;
    }
}

void* Load_Sample(char const* filename)
{
    if (LockedData.DigiHandle == INVALID_AUDIO_HANDLE || filename == nullptr || !Find_File(filename)) {
        return nullptr;
    }

    void* data = nullptr;
    int handle = Open_File(filename, 1);

    if (handle != INVALID_FILE_HANDLE) {
        int data_size = File_Size(handle) + sizeof(AUDHeaderType);
        data = malloc(data_size);

        if (data != nullptr) {
            Sample_Read(handle, data, data_size);
        }

        Close_File(handle);
        Misc = data_size;
    }

    return data;
}

static int Sample_Read(int fh, void* buffer, int size)
{
    if (buffer == nullptr || fh == INVALID_AUDIO_HANDLE || size <= sizeof(AUDHeaderType)) {
        return 0;
    }

    AUDHeaderType header;
    int actual_bytes_read = Read_File(fh, &header, sizeof(AUDHeaderType));
    int to_read = std::min<unsigned>(size - sizeof(AUDHeaderType), header.Size);

    actual_bytes_read += Read_File(fh, static_cast<char*>(buffer) + sizeof(AUDHeaderType), to_read);

    memcpy(buffer, &header, sizeof(AUDHeaderType));

    return actual_bytes_read;
}

void Free_Sample(const void* sample)
{
    if (sample != nullptr) {
        free((void*)sample);
    }
}

bool Audio_Init(int bits_per_sample, bool stereo, int rate, bool reverse_channels)
{
    Init_Locked_Data();

    if (!SoundImp_Init(bits_per_sample, stereo, rate, reverse_channels)) {
        return false;
    }

    LockedData.DigiHandle = 1;

    LockedData.UncompBuffer = malloc(UNCOMP_BUFFER_SIZE);

    if (LockedData.UncompBuffer == nullptr) {
        //CCDebugString("Audio_Init - Failed to allocate UncompBuffer.");
        return false;
    }

    // Create placback buffers for all trackers.
    for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
        SampleTrackerType* st = &LockedData.SampleTracker[i];

        st->BitsPerSample = bits_per_sample;
        st->Stereo = stereo;
        st->Frequency = rate;

        if (!(st->Imp = SoundImp_Init_Sample(bits_per_sample, stereo, rate))) {
            return false;
        }
    }

    SoundType = SFX_ALFX;
    SampleType = SAMPLE_SB;
    AudioDone = false;

    return true;
}

void Sound_End()
{
    if (AudioDone) {
        return;
    }

    for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
        Stop_Sample(i);
        SoundImp_Shutdown_Sample(LockedData.SampleTracker[i].Imp);
    }

    if (FileStreamBuffer != nullptr) {
        free((void*)FileStreamBuffer);
        FileStreamBuffer = nullptr;
    }

    SoundImp_Shutdown();

    if (LockedData.UncompBuffer != nullptr) {
        free((void*)LockedData.UncompBuffer);
        LockedData.UncompBuffer = nullptr;
    }

    AudioDone = true;
}

void Stop_Sample(int index)
{
    if (LockedData.DigiHandle != INVALID_AUDIO_HANDLE && index < MAX_SAMPLE_TRACKERS && !AudioDone) {
        SampleTrackerType* st = &LockedData.SampleTracker[index];

        if (st->Active || st->Loading) {
            st->Active = false;

            if (!st->IsScore) {
                st->Original = nullptr;
            }

            st->Priority = 0;

            if (!st->Loading) {
                SoundImp_Stop_Sample(st->Imp);
            }

            st->Loading = false;

            if (st->FileHandle != INVALID_FILE_HANDLE) {
                Close_File(st->FileHandle);
                st->FileHandle = INVALID_FILE_HANDLE;
            }

            st->QueueBuffer = nullptr;
        }
    }
}

bool Sample_Status(int index)
{
    if (index < 0) {
        return false;
    }

    if (AudioDone) {
        return false;
    }

    if (LockedData.DigiHandle == INVALID_AUDIO_HANDLE || index >= MAX_SAMPLE_TRACKERS) {
        return false;
    }

    SampleTrackerType* st = &LockedData.SampleTracker[index];

    if (st->Loading) {
        return true;
    }

    if (!st->Active) {
        return false;
    }

    return SoundImp_Sample_Status(st->Imp);
}

bool Is_Sample_Playing(const void* sample)
{
    if (AudioDone || sample == nullptr) {
        return false;
    }

    for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
        if (sample == LockedData.SampleTracker[i].Original && Sample_Status(i)) {
            return true;
        }
    }

    return false;
}

void Stop_Sample_Playing(const void* sample)
{
    if (sample != nullptr) {
        for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
            if (LockedData.SampleTracker[i].Original == sample) {
                Stop_Sample(i);
                break;
            }
        }
    }
}

int Play_Sample(const void* sample, int priority, int volume, signed short panloc)
{
    return Play_Sample_Handle(sample, priority, volume, panloc, Get_Free_Sample_Handle(priority));
}

static int Attempt_To_Play_Buffer(int id)
{
    SampleTrackerType* st = &LockedData.SampleTracker[id];

    SoundImp_Start_Sample(st->Imp);

    // Playback was started so we set some needed sample tracker values.
    st->Active = true;

    return id;
}

static int Play_Sample_Handle(const void* sample, int priority, int volume, signed short panloc, int id)
{
    if (Any_Locked()) {
        return INVALID_AUDIO_HANDLE;
    }

    if (!AudioDone) {
        if (sample == nullptr || LockedData.DigiHandle == INVALID_AUDIO_HANDLE) {
            return INVALID_AUDIO_HANDLE;
        }

        if (id == INVALID_AUDIO_HANDLE) {
            return INVALID_AUDIO_HANDLE;
        }

        SampleTrackerType* st = &LockedData.SampleTracker[id];

        // Read in the sample's header.
        AUDHeaderType raw_header;
        memcpy(&raw_header, sample, sizeof(raw_header));
        raw_header.Rate = le16toh(raw_header.Rate);
        raw_header.Size = le32toh(raw_header.Size);
        raw_header.UncompSize = le32toh(raw_header.UncompSize);

        // We don't support anything lower than 20000 hz.
        if (raw_header.Rate < 24000 && raw_header.Rate > 20000) {
            raw_header.Rate = 22050;
        }

        // Set up basic sample tracker info.
        st->Compression = SCompressType(raw_header.Compression);
        st->Original = sample;
        st->Odd = 0;
        st->Reducer = 0;
        st->Restart = false;
        st->QueueBuffer = nullptr;
        st->QueueSize = 0;
        st->OriginalSize = raw_header.Size + sizeof(AUDHeaderType);
        st->Priority = priority;
        st->Service = 0;
        st->Remainder = raw_header.Size;
        st->Source = Add_Long_To_Pointer(sample, sizeof(AUDHeaderType));

        // Compression is ADPCM so we need to init it's stream info.
        if (st->Compression == SCOMP_SOS) {
            st->sosinfo.wChannels = (raw_header.Flags & 1) + 1;
            st->sosinfo.wBitSize = raw_header.Flags & 2 ? 16 : 8;
            st->sosinfo.dwCompSize = raw_header.Size;
            st->sosinfo.dwUnCompSize = raw_header.Size * (st->sosinfo.wBitSize / 4);
            sosCODECInitStream(&st->sosinfo);
        }

        // If the loaded sample doesn't match the sample tracker we need to adjust the tracker.
        int new_bits_per_sample = (raw_header.Flags & 2) ? 16 : 8;
        bool new_stereo = (raw_header.Flags & 1) ? true : false;
        if (new_bits_per_sample != st->BitsPerSample || new_stereo != st->Stereo || raw_header.Rate != st->Frequency) {
            st->Active = false;
            st->Service = 0;
            st->MoreSource = false;

            st->BitsPerSample = new_bits_per_sample;
            st->Stereo = (raw_header.Flags & 1) ? true : false;
            st->Frequency = raw_header.Rate;

            SoundImp_Set_Sample_Attributes(st->Imp, st->BitsPerSample, st->Stereo, st->Frequency);
        }

        // If the sample is already playing stop it.
        if (SoundImp_Sample_Status(st->Imp)) {
            st->Active = false;
            st->Service = 0;
            st->MoreSource = false;

            SoundImp_Stop_Sample(st->Imp);
        }

        while (SoundImp_Get_Sample_Free_Buffer_Count(st->Imp)) {

            int bytes_read = Sample_Copy(st,
                                         &st->Source,
                                         &st->Remainder,
                                         &st->QueueBuffer,
                                         &st->QueueSize,
                                         ChunkBuffer,
                                         BUFFER_CHUNK_SIZE,
                                         st->Compression,
                                         nullptr,
                                         nullptr);

            if (bytes_read > 0) {
                SoundImp_Buffer_Sample_Data(st->Imp, ChunkBuffer, bytes_read);
            }

            if (bytes_read == BUFFER_CHUNK_SIZE) {
                st->MoreSource = true;
                st->OneShot = false;
            } else {
                st->MoreSource = false;
                st->OneShot = true;
                break;
            }
        }

        st->Service = 1;

        st->Volume = volume;

        SoundImp_Set_Sample_Volume(st->Imp, LockedData.SoundVolume * st->Volume);

        if (!Start_Primary_Sound_Buffer(false)) {
            //CCDebugString("Play_Sample_Handle - Can't start primary buffer!");
            return INVALID_AUDIO_HANDLE;
        }

        return Attempt_To_Play_Buffer(id);
    }

    return INVALID_AUDIO_HANDLE;
}

int Set_Score_Vol(int volume)
{
    int old = LockedData.ScoreVolume;
    LockedData.ScoreVolume = volume;

    for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
        SampleTrackerType* st = &LockedData.SampleTracker[i];

        if (st->IsScore & st->Active) {
            SoundImp_Set_Sample_Volume(st->Imp, LockedData.ScoreVolume * st->Volume);
        }
    }

    return old;
}

void Fade_Sample(int index, int ticks)
{
    if (Sample_Status(index)) {
        SampleTrackerType* st = &LockedData.SampleTracker[index];

        if (ticks > 0 && !st->Loading) {
            st->Reducer = ((st->Volume / ticks) + 1);
        } else {
            Stop_Sample(index);
        }
    }
}

static int Get_Free_Sample_Handle(int priority)
{
    int index = 0;

    for (index = MAX_SAMPLE_TRACKERS - 1; index >= 0; --index) {
        if (!LockedData.SampleTracker[index].Active && !LockedData.SampleTracker[index].Loading) {
            if (StartingFileStream || !LockedData.SampleTracker[index].IsScore) {
                break;
            }

            StartingFileStream = true;
        }
    }

    if (index < 0) {
        for (index = 0; index < MAX_SAMPLE_TRACKERS && LockedData.SampleTracker[index].Priority > priority; ++index) {
            ;
        }

        if (index == MAX_SAMPLE_TRACKERS) {
            return INVALID_AUDIO_HANDLE;
        }

        Stop_Sample(index);
    }

    if (index == INVALID_AUDIO_HANDLE) {
        return INVALID_AUDIO_HANDLE;
    }

    if (LockedData.SampleTracker[index].FileHandle != INVALID_FILE_HANDLE) {
        Close_File(LockedData.SampleTracker[index].FileHandle);
        LockedData.SampleTracker[index].FileHandle = INVALID_FILE_HANDLE;
    }

    if (LockedData.SampleTracker[index].Original) {
        if (!LockedData.SampleTracker[index].IsScore) {
            LockedData.SampleTracker[index].Original = 0;
        }
    }

    LockedData.SampleTracker[index].IsScore = false;
    return index;
}

int Get_Digi_Handle()
{
    return LockedData.DigiHandle;
}

void Restore_Sound_Buffers()
{
}

bool Set_Primary_Buffer_Format()
{
    return true;
}

bool Start_Primary_Sound_Buffer(bool forced)
{
    if (!GameInFocus) {
        return false;
    }

    return SoundImp_ResumeSound();
}

void Stop_Primary_Sound_Buffer()
{
    for (int i = 0; i < MAX_SAMPLE_TRACKERS; ++i) {
        Stop_Sample(i);
    }

    SoundImp_PauseSound();
}
