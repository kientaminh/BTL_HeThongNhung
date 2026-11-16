#include "diskio.h"

#include "sd_card.h"

DSTATUS
disk_initialize(BYTE physical_drive)
{
    if (physical_drive != 0u)
    {
        return STA_NOINIT;
    }

    return (sd_card_init() == SD_CARD_OK) ? 0u : STA_NOINIT;
}

DSTATUS
disk_status(BYTE physical_drive)
{
    if ((physical_drive != 0u) || !sd_card_is_initialized())
    {
        return STA_NOINIT;
    }

    return 0u;
}

DRESULT
disk_read(BYTE physical_drive, BYTE *p_buffer, DWORD sector, UINT count)
{
    if ((physical_drive != 0u) || (p_buffer == 0) || (count == 0u))
    {
        return RES_PARERR;
    }

    if (!sd_card_is_initialized())
    {
        return RES_NOTRDY;
    }

    return (sd_card_read_blocks(p_buffer, sector, count) == SD_CARD_OK)
         ? RES_OK : RES_ERROR;
}

DRESULT
disk_write(BYTE physical_drive,
           const BYTE *p_buffer,
           DWORD sector,
           UINT count)
{
    if ((physical_drive != 0u) || (p_buffer == 0) || (count == 0u))
    {
        return RES_PARERR;
    }

    if (!sd_card_is_initialized())
    {
        return RES_NOTRDY;
    }

    return (sd_card_write_blocks(p_buffer, sector, count) == SD_CARD_OK)
         ? RES_OK : RES_ERROR;
}

DRESULT
disk_ioctl(BYTE physical_drive, BYTE command, void *p_buffer)
{
    if (physical_drive != 0u)
    {
        return RES_PARERR;
    }

    if (!sd_card_is_initialized())
    {
        return RES_NOTRDY;
    }

    switch (command)
    {
        case CTRL_SYNC:
            return (sd_card_sync() == SD_CARD_OK) ? RES_OK : RES_ERROR;

        case GET_SECTOR_COUNT:
        {
            uint32_t sector_count;

            if (p_buffer == 0)
            {
                return RES_PARERR;
            }

            if (sd_card_get_sector_count(&sector_count) != SD_CARD_OK)
            {
                return RES_ERROR;
            }

            // FatFs defines this IOCTL output as an aligned DWORD.
            *(DWORD *)p_buffer = sector_count;
            return RES_OK;
        }

        case GET_SECTOR_SIZE:
            if (p_buffer == 0)
            {
                return RES_PARERR;
            }
            // FatFs defines this IOCTL output as an aligned WORD.
            *(WORD *)p_buffer = SD_CARD_BLOCK_SIZE;
            return RES_OK;

        case GET_BLOCK_SIZE:
            if (p_buffer == 0)
            {
                return RES_PARERR;
            }
            *(DWORD *)p_buffer = 1u;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}
