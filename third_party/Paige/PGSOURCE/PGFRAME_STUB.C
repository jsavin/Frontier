#include "Paige.h"
#include "pgMemMgr.h"
#include "pgFrame.h"

/* 2025-11-16 Codex: Provide a minimal frame disposer for headless builds. */

#if defined(FRONTIER_TESTS)
#include <stdio.h>
#endif

PG_PASCAL (void) pgDisposeFrames (paige_rec_ptr pg)
{
	memory_ref PG_FAR *frames = NULL;
	long num_frames = 0;

	if (!pg)
		return;

	if (!(pg->flags2 & HAS_PG_FRAMES_BIT))
		return;

	if (pg->exclusions) {
		frames = (memory_ref PG_FAR *)UseMemory(pg->exclusions);
		num_frames = GetMemorySize(pg->exclusions);
	}

	if (pg->exclude_area)
		(void)UseMemory(pg->exclude_area);

#if defined(FRONTIER_TESTS)
	fprintf(stdout,
		"[pg-frame-stub] dispose start pg=%p flags2=0x%lx frames=%ld exclusions=%p exclude_area=%p\n",
		(void *)pg,
		(long)pg->flags2,
		num_frames,
		(void *)pg->exclusions,
		(void *)pg->exclude_area);
	fflush(stdout);
#endif

	while (frames && num_frames > 0) {
		memory_ref ref = *frames;
#if defined(FRONTIER_TESTS)
		fprintf(stdout,
			"[pg-frame-stub] dispose frame_ref=%p\n",
			(void *)ref);
		fflush(stdout);
#endif
		if (ref)
			UnuseAndDispose(ref);
		++frames;
		--num_frames;
	}

	if (pg->exclusions)
		UnuseMemory(pg->exclusions);
	if (pg->exclude_area)
		UnuseMemory(pg->exclude_area);

	pg->flags2 &= (~HAS_PG_FRAMES_BIT);
}
