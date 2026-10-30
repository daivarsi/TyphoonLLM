/*
 * WoozyLLM Vita - main loop.
 *
 * Owns the app state machine (idle / IME / generating), the Vita system
 * keyboard and the inference worker. The UI layer (imgui) only receives data
 * and raises one-shot intent flags; no inference logic lives in it.
 */

#include <stdio.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr/callback.h>
#include <psp2/power.h>

#include <vita2d.h>

#include "dbg.h"
#include "ime.h"
#include "imgui_ui.h"
#include "llama_worker.h"

#define MESSAGE_MAX 1024

int _newlib_heap_size_user = 80 * 1024 * 1024;

enum app_state {
    ST_IDLE,
    ST_IME_RUN,
    ST_GENERATING
};

static unsigned int g_prev_buttons;
static int g_ime_ready;
static char g_init_error[160];
static volatile int g_exit_requested;

static int power_exit_cb(int notifyId, int notifyCount, int notifyArg, void *userData)
{
    (void)notifyCount;
    (void)notifyArg;
    (void)userData;

    /* User pressed PS and the app is being sent to the background / LiveArea.
     * We cannot suspend a homebrew process, so request a clean exit instead of
     * leaving the llama worker + UI running underneath the Live Area. */
    if (notifyId & SCE_POWER_CB_APP_SUSPEND)
        g_exit_requested = 1;
    return 0;
}

static void install_power_exit_handler(void)
{
    SceUID cbid = sceKernelCreateCallback("power_exit", 0, power_exit_cb, NULL);
    if (cbid >= 0)
        scePowerRegisterCallback(cbid);
}

static int open_keyboard(void)
{
    int ret = ime_open("WoozyLLM", "");
    if (ret < 0) {
        g_ime_ready = 0;
        snprintf(g_init_error, sizeof(g_init_error),
                 "IME open error: 0x%08X", (unsigned int)ret);
        imgui_ui_set_status(g_init_error);
        dbg_log("main: %s", g_init_error);
        return ret;
    }
    imgui_ui_set_status("Keyboard is open...");
    return 0;
}

static void submit_prompt(enum app_state *state, const char *prompt)
{
    /* batches BOTH the User message and the empty Assistant placeholder, so
     * the "typing" bubble exists the moment we submit and a streamed token
     * can never land inside the User bubble. */
    if (!imgui_ui_send_message(prompt)) {
        imgui_ui_set_status("Wait for the current answer to finish.");
        return;                    /* no double submit while generating */
    }

    enum llama_worker_state ws = llama_worker_get_state();
    if (ws == LLAMA_WORKER_READY || ws == LLAMA_WORKER_DONE) {
        if (llama_worker_start(prompt) == 0) {
            imgui_ui_set_status("Generating... UI stays responsive");
            *state = ST_GENERATING;
        } else {
            imgui_ui_stream_assistant(llama_worker_get_error());
            imgui_ui_end_assistant();
            imgui_ui_set_status("Could not start generation.");
            *state = ST_IDLE;
        }
    } else {
        /* model is still loading or crashed - keep the placeholder text
         * meaningful instead of leaving an empty bubble and a stuck state. */
        imgui_ui_stream_assistant(ws == LLAMA_WORKER_ERROR ? llama_worker_get_error()
                                                           : "Model is not ready yet.");
        imgui_ui_end_assistant();
        imgui_ui_set_status(ws == LLAMA_WORKER_ERROR ? llama_worker_get_error()
                                                     : "Model is not ready yet.");
        *state = ST_IDLE;
    }
}

int main(void)
{
    dbg_init();
    dbg_log("main: started");

    install_power_exit_handler();

    // raise the clocks to speed up inference (PCH-1000 arm / PCH-2000 bus)
    scePowerSetArmClockFrequency(444000000);
    scePowerSetBusClockFrequency(222000000);
    scePowerSetGpuXbarClockFrequency(222000000);

    int ime_rc = ime_init();
    if (ime_rc < 0) {
        g_ime_ready = 0;
        snprintf(g_init_error, sizeof(g_init_error),
                 "IME init error: 0x%08X", (unsigned int)ime_rc);
        imgui_ui_set_status(g_init_error);
        dbg_log("main: %s", g_init_error);
    } else {
        g_ime_ready = 1;
    }

    unsigned long long bench_load_start=sceKernelGetProcessTimeWide();
    int llama_rc = llama_worker_init();
    dbg_log("BENCH load_us=%llu cpu_MHz=%d bus_MHz=%d",(unsigned long long)(sceKernelGetProcessTimeWide()-bench_load_start),scePowerGetArmClockFrequency(),scePowerGetBusClockFrequency());
    if (llama_rc < 0) {
        imgui_ui_set_status("Copy model.bin and tokenizer.bin to ux0:data/woozyllm/");
        dbg_log("main: llama_worker_init failed (%d)", llama_rc);
    } else {
        char status[192];
        snprintf(status, sizeof(status), "%s", llama_worker_get_model_info());
        imgui_ui_set_status(status);
    }

    if (vita2d_init() < 0) {
        dbg_log("main: vita2d_init failed");
        return -1;
    }
    vita2d_set_clear_color(RGBA8(0, 0, 0, 255));        /* pure black chat area */

    if (imgui_ui_init() < 0) {
        dbg_log("main: imgui_ui_init failed");
        vita2d_fini();
        return -1;
    }

    if (sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG) < 0) {
        dbg_log("main: sceCtrlSetSamplingMode failed");
        imgui_ui_fini();
        vita2d_fini();
        return -1;
    }

    enum app_state state = ST_IDLE;
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));

    unsigned int frame = 0;

    const char *bench_prompts[]={"Explain what gravity is in one sentence.","What is 17 + 28?","Write three facts about the Moon.","Give me a short recipe for scrambled eggs.","If Alice has 5 apples and gives Bob 2, how many does she have left?"};
    int bench_index=0; unsigned int bench_next=120;
    for (;;) {
        if (state==ST_IDLE && frame>=bench_next && bench_index<5 && llama_rc==0) {
            int prompt_index=bench_index;
            dbg_log("BENCH begin run=%d prompt_index=%d",bench_index+1,prompt_index);
            submit_prompt(&state,bench_prompts[prompt_index]);
            bench_index++;bench_next=frame+120;
        }
        if (g_exit_requested) {
            dbg_log("main: exiting (LiveArea / suspend). Stopping worker.");
            llama_worker_stop();
            sceKernelDelayThread(500000);
            sceKernelExitProcess(0);
        }

        if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0) {
            g_prev_buttons = pad.buttons;

            switch (state) {
            case ST_IDLE:
                if (imgui_ui_want_input()) {
                    imgui_ui_clear_want_input();
                    if (g_ime_ready && open_keyboard() == 0)
                        state = ST_IME_RUN;
                } else if (imgui_ui_want_send()) {
                    imgui_ui_clear_want_send();
                    const char *prompt = imgui_ui_take_draft();
                    if (prompt[0])
                        submit_prompt(&state, prompt);
                }
                break;

            case ST_IME_RUN:
                if (ime_get_status() == IME_STATUS_RUNNING) {
                    /* LIVE: push the current IME text into the UI draft so
                     * the input field previews the typed text in real time
                     * while the keyboard is still open. */
                    imgui_ui_set_draft(ime_get_text());
                } else {
                    const char *text = ime_get_text();
                    ime_term();
                    if (text[0])
                        imgui_ui_set_draft(text);
                    else
                        imgui_ui_set_status("");
                    state = ST_IDLE;
                }
                break;

            case ST_GENERATING:
            {
                if (imgui_ui_want_stop()) {
                    imgui_ui_clear_want_stop();
                    llama_worker_stop();
                }
                enum llama_worker_state ws = llama_worker_get_state();
                if (ws == LLAMA_WORKER_RUNNING) {
                    imgui_ui_stream_assistant(llama_worker_get_response());
                } else if (ws == LLAMA_WORKER_DONE) {
                    imgui_ui_stream_assistant(llama_worker_get_response());
                    imgui_ui_end_assistant();
                    imgui_ui_set_status("");
                    state = ST_IDLE;
                } else if (ws == LLAMA_WORKER_ERROR) {
                    imgui_ui_stream_assistant(llama_worker_get_error());
                    imgui_ui_end_assistant();
                    imgui_ui_set_status(llama_worker_get_error());
                    state = ST_IDLE;
                }
                break;
            }
            }
        }

        vita2d_start_drawing();
        vita2d_clear_screen();
        imgui_ui_begin();
        imgui_ui_draw();
        imgui_ui_end();
        vita2d_end_drawing();

        if (state == ST_IME_RUN)
            vita2d_common_dialog_update();

        vita2d_swap_buffers();

        frame++;
        if ((frame % 120) == 0) {
            enum llama_worker_state ws = llama_worker_get_state();
            dbg_log("main: frame=%u state=%d ws=%d", frame, (int)state, (int)ws);
        }
    }
}