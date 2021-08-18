/*
 * X.org screen grabber for MJPG-streamer
 * Based on Linux-UVC streaming input-plugin for MJPG-streamer
 * 2021 Ilya Chelyadin (C)
 */

#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define OUTPUT_BUF_SIZE  4096

typedef struct {
    struct jpeg_destination_mgr pub; /* public fields */

    JOCTET * buffer;    /* start of buffer */

    unsigned char *outbuffer;
    int outbuffer_size;
    unsigned char *outbuffer_cursor;
    int *written;

} mjpg_destination_mgr;

typedef mjpg_destination_mgr * mjpg_dest_ptr;

/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) init_destination(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    /* Allocate the output buffer --- it will be released when done with image */
    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));

    *(dest->written) = 0;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/******************************************************************************
Description.: called whenever local jpeg buffer fills up
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(boolean) empty_output_buffer(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    memcpy(dest->outbuffer_cursor, dest->buffer, OUTPUT_BUF_SIZE);
    dest->outbuffer_cursor += OUTPUT_BUF_SIZE;
    *(dest->written) += OUTPUT_BUF_SIZE;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

/******************************************************************************
Description.: called by jpeg_finish_compress after all data has been written.
              Usually needs to flush buffer.
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) term_destination(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    memcpy(dest->outbuffer_cursor, dest->buffer, datacount);
    dest->outbuffer_cursor += datacount;
    *(dest->written) += datacount;
}

/******************************************************************************
Description.: Prepare for output to a stdio stream.
Input Value.: buffer is the already allocated buffer memory that will hold
              the compressed picture. "size" is the size in bytes.
Return Value: -
******************************************************************************/
GLOBAL(void) dest_buffer(j_compress_ptr cinfo, unsigned char *buffer, int size, int *written)
{
    mjpg_dest_ptr dest;

    if(cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(mjpg_destination_mgr));
    }

    dest = (mjpg_dest_ptr) cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outbuffer = buffer;
    dest->outbuffer_size = size;
    dest->outbuffer_cursor = buffer;
    dest->written = written;
}


int save_jpg(int8_t* buffer, uint16_t width, uint16_t height, int quality, unsigned char *outbuffer) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    static int written;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    dest_buffer(&cinfo, outbuffer, (int)(width*height*8), &written);
    cinfo.image_width      = width;
    cinfo.image_height     = height;
    cinfo.input_components = 3;
    cinfo.in_color_space   = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality (&cinfo, quality, true);
    jpeg_start_compress(&cinfo, true);

    JSAMPROW row_pointer;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer = (JSAMPROW) &buffer[cinfo.next_scanline * cinfo.input_components * cinfo.image_width];
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return (written);
}
