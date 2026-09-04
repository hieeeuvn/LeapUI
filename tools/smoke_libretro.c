/*
 * smoke_libretro.c - frontend libretro toi gian de smoke-test core build PC (Linux/WSL).
 *
 * Build:
 *   gcc -O0 -g -Wall -I<repo>/include tools/smoke_libretro.c -o /tmp/smoke_libretro -ldl
 *
 * Chay (sandbox phai co ROMS/"Game Boy Advance" chua file .gba + system/assets/LeapUI/):
 *   /tmp/smoke_libretro <duong-dan>/leapui_libretro.so <sandbox>
 *
 * Core tu ghi log vao <sandbox>/system/assets/LeapUI/leapui.log (dong shelf_scan ... count=N).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dlfcn.h>

#include "libretro.h"
#include "dartos.h" /* RETRO_ENVIRONMENT_GET_ROMS_DIRECTORY (PRIVATE|1), RUN_EMULATOR... */

static const char *g_roms  = NULL;
static const char *g_assets = NULL;
static const char *g_save  = NULL;
static int g_frame = 0;
static int g_fb_w = 0, g_fb_h = 0, g_fb_pitch = 0;
static const void *g_fb_data = NULL;
static int g_polls = 0;

static bool env_cb(unsigned cmd, void *data)
{
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
         printf("[env] SET_PIXEL_FORMAT (%s)\n",
                *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_RGB565 ? "RGB565" : "other");
         return true;
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         return true;
      case RETRO_ENVIRONMENT_GET_ROMS_DIRECTORY:
         if (g_roms)  { *(const char **)data = g_roms;  return true; }
         break;
      case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
         if (g_assets) { *(const char **)data = g_assets; return true; }
         break;
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         if (g_save)  { *(const char **)data = g_save;  return true; }
         break;
      default:
         break;
   }
   return false;
}

static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch)
{
   g_fb_data = data;
   if (!g_frame) { g_fb_w = w; g_fb_h = h; g_fb_pitch = (int)pitch; }
   g_frame++;
}

static void audio_cb(int16_t left, int16_t right) { (void)left; (void)right; }
static size_t audio_batch_cb(const int16_t *data, size_t frames)
{ (void)data; return frames; }
static void input_poll_cb(void) { g_polls++; }
static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)port; (void)device; (void)index;
   /* mo phong bam A 1 frame (poll #3) -> kich hoat queue_insert + ROM/CORE check trong log */
   if (g_polls == 3 && id == RETRO_DEVICE_ID_JOYPAD_A) return 1;
   return 0;
}

#define LOAD(sym)                                                               \
   do {                                                                          \
      *(void **)(&sym) = dlsym(core, #sym);                                      \
      if (!sym) { fprintf(stderr, "[FAIL] thieu symbol %s: %s\n", #sym, dlerror()); return 2; } \
   } while (0)

int main(int argc, char **argv)
{
   if (argc < 3)
   {
      fprintf(stderr, "dung: %s <core.so> <sandbox> [framedump.rgb565]\n", argv[0]);
      return 2;
   }

   char roms_dir[512], assets_dir[512], save_dir[512];
   snprintf(roms_dir, sizeof(roms_dir), "%s/ROMS", argv[2]);
   snprintf(assets_dir, sizeof(assets_dir), "%s/system/assets/LeapUI", argv[2]);
   snprintf(save_dir, sizeof(save_dir), "%s/Saves", argv[2]);
   g_roms = roms_dir; g_assets = assets_dir; g_save = save_dir;

   void *core = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   if (!core) { fprintf(stderr, "[FAIL] dlopen: %s\n", dlerror()); return 2; }
   printf("[OK] dlopen %s\n", argv[1]);

   void (*retro_set_environment)(retro_environment_t) = NULL;
   void (*retro_set_video_refresh)(retro_video_refresh_t) = NULL;
   void (*retro_set_audio_sample)(retro_audio_sample_t) = NULL;
   void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
   void (*retro_set_input_poll)(retro_input_poll_t) = NULL;
   void (*retro_set_input_state)(retro_input_state_t) = NULL;
   void (*retro_init)(void) = NULL;
   void (*retro_deinit)(void) = NULL;
   bool (*retro_load_game)(const struct retro_game_info *) = NULL;
   void (*retro_unload_game)(void) = NULL;
   void (*retro_run)(void) = NULL;
   void (*retro_get_system_info)(struct retro_system_info *) = NULL;
   LOAD(retro_set_environment); LOAD(retro_set_video_refresh);
   LOAD(retro_set_audio_sample); LOAD(retro_set_audio_sample_batch);
   LOAD(retro_set_input_poll); LOAD(retro_set_input_state);
   LOAD(retro_init); LOAD(retro_deinit); LOAD(retro_load_game);
   LOAD(retro_unload_game); LOAD(retro_run); LOAD(retro_get_system_info);

   retro_set_environment(env_cb);
   retro_set_video_refresh(video_cb);
   retro_set_audio_sample(audio_cb);
   retro_set_audio_sample_batch(audio_batch_cb);
   retro_set_input_poll(input_poll_cb);
   retro_set_input_state(input_state_cb);

   struct retro_system_info si;
   memset(&si, 0, sizeof(si));
   retro_get_system_info(&si);
   printf("[info] library=%s v%s ext=\"%s\" need_fullpath=%d\n",
          si.library_name, si.library_version,
          si.valid_extensions ? si.valid_extensions : "(null)", si.need_fullpath);

   retro_init();

   char rom_path[600];
   snprintf(rom_path, sizeof(rom_path), "%s/Game Boy Advance/a.gba", g_roms);
   struct retro_game_info gi;
   memset(&gi, 0, sizeof(gi));
   gi.path = rom_path;
   if (!retro_load_game(&gi))
   { fprintf(stderr, "[FAIL] retro_load_game\n"); return 3; }
   printf("[OK] retro_load_game\n");

   const int FRAMES = 10;
   for (int i = 0; i < FRAMES; i++) retro_run();
   printf("[OK] retro_run x%d (video cb: %d frame, %dx%d pitch=%d, input poll x%d)\n",
          FRAMES, g_frame, g_fb_w, g_fb_h, g_fb_pitch, g_polls);
   if (g_frame == 0)
   { fprintf(stderr, "[FAIL] core khong goi video_refresh\n"); return 4; }

   /* dump frame RGB565 cuoi cung (truoc khi core giai phong framebuffer) */
   if (argc > 3 && g_fb_data && g_fb_pitch == g_fb_w * 2)
   {
      FILE *d = fopen(argv[3], "wb");
      if (d)
      {
         if (fwrite(g_fb_data, (size_t)g_fb_w * g_fb_h * 2, 1, d) == 1)
            printf("[dump] %s (%dx%d RGB565)\n", argv[3], g_fb_w, g_fb_h);
         else
            fprintf(stderr, "[WARN] ghi frame that bai\n");
         fclose(d);
      }
   }

   retro_unload_game();
   retro_deinit();
   dlclose(core);

   /* log cua core nam ngay trong assets dir */
   char log_path[600];
   snprintf(log_path, sizeof(log_path), "%s/leapui.log", g_assets);
   FILE *f = fopen(log_path, "rb");
   if (!f) { fprintf(stderr, "[WARN] khong doc duoc %s\n", log_path); return 0; }
   char line[512];
   printf("---- %s ----\n", log_path);
   while (fgets(line, sizeof(line), f)) printf("  %s", line);
   fclose(f);

   printf("== SMOKE TEST DONE ==\n");
   return 0;
}
