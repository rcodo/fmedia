/** MPEG input.
Copyright (c) 2015 Simon Zolin */

#include <fmedia.h>

#include <FF/audio/id3.h>
#include <FF/audio/mpeg.h>
#include <FF/audio/mp3lame.h>
#include <FF/audio/pcm.h>
#include <FF/array.h>


static const fmed_core *core;

static const char *const metanames[] = {
	NULL
	, "=meta_comment"
	, "=meta_album"
	, "=meta_genre"
	, "=meta_title"
	, NULL
	, "=meta_artist"
	, NULL
	, "=meta_tracknumber"
	, "=meta_date"
};

typedef struct fmed_mpeg {
	ffid3 id3;
	ffmpg mpg;
	ffstr title
		, artist;
	byte meta[FFCNT(metanames)];
	uint state;
} fmed_mpeg;

typedef struct mpeg_out {
	uint state;
	ffmpg_enc mpg;
} mpeg_out;

static struct mpeg_out_conf_t {
	uint qual;
} mpeg_out_conf;

//FMEDIA MODULE
static const void* mpeg_iface(const char *name);
static int mpeg_sig(uint signo);
static void mpeg_destroy(void);
static const fmed_mod fmed_mpeg_mod = {
	&mpeg_iface, &mpeg_sig, &mpeg_destroy
};

//DECODE
static void* mpeg_open(fmed_filt *d);
static void mpeg_close(void *ctx);
static int mpeg_process(void *ctx, fmed_filt *d);
static const fmed_filter fmed_mpeg_input = {
	&mpeg_open, &mpeg_process, &mpeg_close
};

//ENCODE
static void* mpeg_out_open(fmed_filt *d);
static void mpeg_out_close(void *ctx);
static int mpeg_out_process(void *ctx, fmed_filt *d);
static int mpeg_out_config(ffpars_ctx *ctx);
static const fmed_filter fmed_mpeg_output = {
	&mpeg_out_open, &mpeg_out_process, &mpeg_out_close, &mpeg_out_config
};

static const ffpars_arg mpeg_out_conf_args[] = {
	{ "quality",	FFPARS_TINT,  FFPARS_DSTOFF(struct mpeg_out_conf_t, qual) },
};


FF_EXP const fmed_mod* fmed_getmod(const fmed_core *_core)
{
	ffmem_init();
	core = _core;
	return &fmed_mpeg_mod;
}


static const void* mpeg_iface(const char *name)
{
	if (!ffsz_cmp(name, "decode"))
		return &fmed_mpeg_input;
	else if (!ffsz_cmp(name, "encode"))
		return &fmed_mpeg_output;
	return NULL;
}

static int mpeg_sig(uint signo)
{
	return 0;
}

static void mpeg_destroy(void)
{
}


static void* mpeg_open(fmed_filt *d)
{
	int64 total_size;
	fmed_mpeg *m = ffmem_tcalloc1(fmed_mpeg);
	if (m == NULL)
		return NULL;
	ffmpg_init(&m->mpg);

	m->mpg.seekable = 1;
	if (FMED_NULL != (total_size = fmed_getval("total_size")))
		m->mpg.total_size = total_size;

	ffid3_parseinit(&m->id3);
	return m;
}

static void mpeg_close(void *ctx)
{
	fmed_mpeg *m = ctx;
	ffstr_free(&m->title);
	ffstr_free(&m->artist);
	ffid3_parsefin(&m->id3);
	ffmpg_close(&m->mpg);
	ffmem_free(m);
}

static int mpeg_meta(fmed_mpeg *m, fmed_filt *d)
{
	ffstr3 val = {0};
	uint tag;

	for (;;) {
		size_t len = d->datalen;
		int r = ffid3_parse(&m->id3, d->data, &len);
		d->data += len;
		d->datalen -= len;

		switch (r) {
		case FFID3_RDONE:
			ffid3_parsefin(&m->id3);
			return FMED_RDONE;

		case FFID3_RERR:
			errlog(core, d->trk, "mpeg", "id3: parse (offset: %u): ID3v2.%u.%u, flags: %u, size: %u"
				, sizeof(ffid3_hdr) + ffid3_size(&m->id3.h) - m->id3.size
				, (uint)m->id3.h.ver[0], (uint)m->id3.h.ver[1], (uint)m->id3.h.flags
				, ffid3_size(&m->id3.h));
			break;

		case FFID3_RMORE:
			return FMED_RMORE;

		case FFID3_RHDR:
			m->mpg.dataoff = sizeof(ffid3_hdr) + ffid3_size(&m->id3.h);

			dbglog(core, d->trk, "mpeg", "id3: ID3v2.%u.%u, size: %u"
				, (uint)m->id3.h.ver[0], (uint)m->id3.h.ver[1], ffid3_size(&m->id3.h));
			break;

		case FFID3_RFRAME:
			switch (ffid3_frame(&m->id3.fr)) {
			case FFID3_PICTURE:
			case FFID3_COMMENT:
				m->id3.flags &= ~FFID3_FWHOLE;
				break;

			default:
				m->id3.flags |= FFID3_FWHOLE;
			}
			break;

		case FFID3_RDATA:
			if (!(m->id3.flags & FFID3_FWHOLE))
				break;

			if (0 > ffid3_getdata(m->id3.data.ptr, m->id3.data.len, m->id3.txtenc, 0, &val)) {
				errlog(core, d->trk, "mpeg", "id3: get frame data");
				break;
			}
			dbglog(core, d->trk, "mpeg", "tag: %*s: %S", (size_t)4, m->id3.fr.id, &val);

			tag = ffid3_frame(&m->id3.fr);
			if (tag < FFCNT(metanames) && metanames[tag] != NULL && val.len != 0) {
				m->meta[tag] = 1;
				d->track->setvalstr(d->trk, metanames[tag], ffsz_alcopy(val.ptr, val.len));
			}

			if (tag == FFID3_LENGTH && m->id3.data.len != 0) {
				uint64 dur;
				if (m->id3.data.len == ffs_toint(m->id3.data.ptr, m->id3.data.len, &dur, FFS_INT64))
					m->mpg.total_len = dur;
			}

			ffarr_free(&val);
			break;
		}
	}

	return 0; //unreachable
}

static int mpeg_process(void *ctx, fmed_filt *d)
{
	enum { I_META, I_HDR, I_DATA };
	fmed_mpeg *m = ctx;
	int r;
	int64 seek_time;

	if (d->flags & FMED_FSTOP) {
		d->outlen = 0;
		return FMED_RLASTOUT;
	}

again:
	switch (m->state) {
	case I_META:
		r = mpeg_meta(m, d);
		if (r != FMED_RDONE)
			return r;
		m->state = I_HDR;
		//break;

	case I_HDR:
		break;

	case I_DATA:
		if (FMED_NULL != (seek_time = fmed_popval("seek_time")))
			ffmpg_seek(&m->mpg, ffpcm_samples(seek_time, m->mpg.fmt.sample_rate));
		break;
	}

	m->mpg.data = d->data;
	m->mpg.datalen = d->datalen;

	for (;;) {
		r = ffmpg_decode(&m->mpg);

		switch (r) {
		case FFMPG_RDATA:
			goto data;

		case FFMPG_RMORE:
			if (d->flags & FMED_FLAST) {
				d->outlen = 0;
				return FMED_RDONE;
			}
			return FMED_RMORE;

		case FFMPG_RHDR:
			fmed_setpcm(d, &m->mpg.fmt);
			d->track->setvalstr(d->trk, "pcm_decoder", "MPEG");
			fmed_setval("pcm_ileaved", 0);
			fmed_setval("bitrate", m->mpg.bitrate);
			fmed_setval("total_samples", m->mpg.total_samples);
			m->state = I_DATA;
			goto again;

		case FFMPG_RTAG: {
			int tag = m->mpg.tagframe;
			if (tag < FFCNT(metanames) && metanames[tag] != NULL && m->mpg.tagval.len != 0
				&& m->meta[tag] == 0) {
				d->track->setvalstr(d->trk, metanames[tag], ffsz_alcopy(m->mpg.tagval.ptr, m->mpg.tagval.len));
			}
			}
			break;

		case FFMPG_RSEEK:
			fmed_setval("input_seek", ffmpg_seekoff(&m->mpg));
			return FMED_RMORE;

		case FFMPG_RWARN:
			errlog(core, d->trk, "mpeg", "warning: ffmpg_decode(): %s", ffmpg_errstr(&m->mpg));
			break;

		case FFMPG_RERR:
		default:
			errlog(core, d->trk, "mpeg", "ffmpg_decode(): %s", ffmpg_errstr(&m->mpg));
			return FMED_RERR;
		}
	}

data:
	d->data = m->mpg.data;
	d->datalen = m->mpg.datalen;
	d->outni = (void**)m->mpg.pcm;
	d->outlen = m->mpg.pcmlen;
	fmed_setval("current_position", ffmpg_cursample(&m->mpg));

	dbglog(core, d->trk, "mpeg", "output: %L PCM samples"
		, d->outlen / ffpcm_size1(&m->mpg.fmt));
	return FMED_ROK;
}


static int mpeg_out_config(ffpars_ctx *ctx)
{
	mpeg_out_conf.qual = 2;
	ffpars_setargs(ctx, &mpeg_out_conf, mpeg_out_conf_args, FFCNT(mpeg_out_conf_args));
	return 0;
}

static void* mpeg_out_open(fmed_filt *d)
{
	mpeg_out *m = ffmem_tcalloc1(mpeg_out);
	if (m == NULL)
		return NULL;
	return m;
}

static void mpeg_out_close(void *ctx)
{
	mpeg_out *m = ctx;
	ffmpg_enc_close(&m->mpg);
	ffmem_free(m);
}

static int mpeg_out_process(void *ctx, fmed_filt *d)
{
	mpeg_out *m = ctx;
	ffpcm pcm;
	int r, qual;

	switch (m->state) {
	case 0:
	case 1:
		pcm.format = (int)fmed_getval("pcm_format");
		pcm.sample_rate = (int)fmed_getval("pcm_sample_rate");
		pcm.channels = (int)fmed_getval("pcm_channels");

		if (FMED_NULL == (qual = (int)fmed_getval("mpeg-quality")))
			qual = mpeg_out_conf.qual;

		m->mpg.ileaved = fmed_getval("pcm_ileaved");
		if (0 != (r = ffmpg_create(&m->mpg, &pcm, qual))) {

			if (r == FFMPG_EFMT && m->state == 0) {
				fmed_setval("conv_pcm_format", FFPCM_16LE);
				m->state = 1;
				return FMED_RMORE;
			}

			errlog(core, d->trk, "mpeg", "ffmpg_create() failed: %s", ffmpg_enc_errstr(&m->mpg));
			return FMED_RERR;
		}
		m->state = 2;
		break;
	}

	m->mpg.pcm = (void*)d->data;
	m->mpg.pcmlen = d->datalen;

	for (;;) {
		r = ffmpg_encode(&m->mpg);
		switch (r) {

		case FFMPG_RDATA:
			goto data;

		case FFMPG_RMORE:
			if (!(d->flags & FMED_FLAST)) {
				return FMED_RMORE;
			}
			m->mpg.fin = 1;
			break;

		case FFMPG_RSEEK:
			fmed_setval("output_seek", 0);
			break;

		case FFMPG_RDONE:
			d->outlen = 0;
			return FMED_RDONE;

		default:
			errlog(core, d->trk, "mpeg", "ffmpg_encode() failed: %s", ffmpg_enc_errstr(&m->mpg));
			return FMED_RERR;
		}
	}

data:
	d->out = m->mpg.data;
	d->outlen = m->mpg.datalen;
	d->datalen = m->mpg.pcmlen;

	dbglog(core, d->trk, "mpeg", "output: %L bytes"
		, m->mpg.datalen);
	return FMED_ROK;
}
