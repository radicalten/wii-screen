#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#define FIFO_SIZE (256*1024)

static void *xfb = NULL;
static GXRModeObj rmode;

static void setup_gx()
{
    GXColor backgroundColor = {0, 0, 0, 255};
    void *fifoBuffer = NULL;
    Mtx mv;

    fifoBuffer = MEM_K0_TO_K1(memalign(32,FIFO_SIZE));
    memset(fifoBuffer, 0, FIFO_SIZE);

    GX_Init(fifoBuffer, FIFO_SIZE);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    GX_SetCullMode(GX_CULL_NONE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetCopyClear(backgroundColor, GX_MAX_Z24);

    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0,
                   GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);


    guMtxIdentity(mv);
    guMtxTransApply(mv, mv, 0.4, 0.4, 0);
    GX_LoadPosMtxImm(mv, GX_PNMTX0);
}

static void setup_viewport()
{
    Mtx44 proj;
    u32 w, h;

    w = rmode.fbWidth;
    h = rmode.efbHeight;

    // matrix, t, b, l, r, n, f
    guOrtho(proj, 0, h, 0, w, 0, 1);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

    GX_SetViewport(0, 0, w, h, 0, 1);

    GX_SetDispCopyYScale((f32)rmode.xfbHeight / (f32)h);
    GX_SetScissor(0, 0, w, h);
    GX_SetDispCopySrc(0, 0, w, h);
    GX_SetDispCopyDst(w, rmode.xfbHeight);
    GX_SetCopyFilter(rmode.aa, rmode.sample_pattern, GX_TRUE, rmode.vfilter);
}

static void draw_line(int x0, int y0, int x1, int y1)
{
    GX_Begin(GX_LINES, GX_VTXFMT0, 2);

    GX_Position2s16(x0, y0);
    GX_Color4u8(255, 255, 255, 64);
    GX_Position2s16(x1, y1);
    GX_Color4u8(255, 255, 255, 64);

    GX_End();
}

static void draw_background()
{
    u32 w, h;

    w = rmode.fbWidth;
    h = rmode.efbHeight;

    for (int x = 0; x < w; x += 16) {
        draw_line(x, 0, x, h);
    }
    draw_line(w - 1, 0, w - 1, h);

    for (int y = 0; y < h; y += 16) {
        draw_line(0, y, w, y);
    }
    draw_line(0, h - 1, w, h - 1);
}

int main(int argc, char **argv)
{
    const GXRModeObj *vmode;

	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	WPAD_Init();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	vmode = VIDEO_GetPreferredMode(NULL);
    memcpy(&rmode, vmode, sizeof(rmode));

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(&rmode));

	// Set up the video registers with the chosen mode
	VIDEO_Configure(&rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if (rmode.viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

    setup_gx();
    setup_viewport();

	while(1) {

		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = WPAD_ButtonsDown(0);

		// We return to the launcher application via exit
		if ( pressed & WPAD_BUTTON_HOME ) exit(0);

        draw_background();

        GX_DrawDone();
        GX_CopyDisp(xfb, GX_TRUE);
        GX_Flush();

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}
