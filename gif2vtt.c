#include <gif_lib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define DEFAULT_TILE_SIZE 1
#define PROGRESS_BAR_WIDTH 50

typedef struct {
    int width, height;
    uint8_t *pixels;
    float duration;
} FrameData;

void write_frame_to_vtt(FILE *f, FrameData *frame, int tile_size, float start_time) {
    float end_time = start_time + frame->duration;

    fprintf(f,"%02d:%02d:%06.3f --> %02d:%02d:%06.3f\n",
            (int)(start_time/3600),(int)((int)start_time%3600)/60,start_time - ((int)start_time/60)*60,
            (int)(end_time/3600),(int)((int)end_time%3600)/60,end_time - ((int)end_time/60)*60
    );

    fprintf(f,"<size=5>\n");

    for(int y=0; y<frame->height; y+=tile_size){
        for(int x=0; x<frame->width; x+=tile_size){
            int idx = (y*frame->width + x)*4;
            uint8_t r = frame->pixels[idx];
            uint8_t g = frame->pixels[idx+1];
            uint8_t b = frame->pixels[idx+2];
            uint8_t a = frame->pixels[idx+3];

            if(a < 10)
                fputc(' ', f);
            else
                fprintf(f,"<color=#%02X%02X%02X>█</color>", r,g,b);
        }
        fputc('\n', f);
    }

    fprintf(f,"</size>\n\n");
}

void print_progress(int current, int total) {
    float fraction = (float)current / total;
    int filled = fraction * PROGRESS_BAR_WIDTH;

    printf("\rFrame %d/%d [", current, total);
    for(int i=0; i<PROGRESS_BAR_WIDTH; i++){
        if(i<filled) printf("=");
        else printf(" ");
    }
    printf("]");
    fflush(stdout);
}

int main(int argc, char **argv){
    if(argc < 5){
        printf("Usage: %s <input.gif> <output.vtt> <loop_count> <tile_size>\n", argv[0]);
        printf("Example: %s mygif.gif output.vtt 3 1\n", argv[0]);
        return 1;
    }

    const char *input = argv[1];
    const char *output = argv[2];
    int loop_count = atoi(argv[3]);
    int tile_size = atoi(argv[4]);
    if(tile_size <= 0) tile_size = DEFAULT_TILE_SIZE;

    int error;
    GifFileType *gif = DGifOpenFileName(input,&error);
    if(!gif){ fprintf(stderr,"Failed to open GIF: %s\n", input); return 1; }
    if(DGifSlurp(gif) != GIF_OK){ fprintf(stderr,"Failed to read GIF\n"); return 1; }

    FILE *f = fopen(output,"w");
    if(!f){ perror("fopen"); return 1; }
    fprintf(f,"WEBVTT\n\n");

    int total_frames = gif->ImageCount;

    for(int loop = 0; loop < loop_count; loop++){
        float loop_offset = 0.0f;
        for(int i=0; i<total_frames; i++){
            SavedImage *img = &gif->SavedImages[i];
            int w = gif->SWidth;
            int h = gif->SHeight;

            FrameData frame;
            frame.width = w;
            frame.height = h;
            frame.pixels = malloc(w*h*4);
            if(!frame.pixels){ fprintf(stderr,"Memory allocation failed\n"); return 1; }
            frame.duration = 0.1f;

            for(int j=0;j<img->ExtensionBlockCount;j++){
                ExtensionBlock *ext = &img->ExtensionBlocks[j];
                if(ext->Function == GRAPHICS_EXT_FUNC_CODE && ext->ByteCount >= 4){
                    int delay = ext->Bytes[1] | (ext->Bytes[2]<<8);
                    frame.duration = delay / 100.0f;
                }
            }

            // Convert indexed color to RGBA
            ColorMapObject *cmap = img->ImageDesc.ColorMap ? img->ImageDesc.ColorMap : gif->SColorMap;
            for(int y=0; y<h; y++){
                for(int x=0; x<w; x++){
                    int idx = y*w + x;
                    GifByteType color_idx = img->RasterBits[idx];
                    GifColorType color = cmap->Colors[color_idx];
                    frame.pixels[idx*4 + 0] = color.Red;
                    frame.pixels[idx*4 + 1] = color.Green;
                    frame.pixels[idx*4 + 2] = color.Blue;
                    frame.pixels[idx*4 + 3] = 255;
                }
            }

            write_frame_to_vtt(f, &frame, tile_size, loop_offset);
            loop_offset += frame.duration;
            free(frame.pixels);

            print_progress(i+1, total_frames);
        }
    }

    fclose(f);
    DGifCloseFile(gif,&error);
    printf("\nDone! VTT saved to %s\n", output);
    return 0;
}
