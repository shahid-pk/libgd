#include <config.h>
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "gd.h"

#include "gdtest.h"
#include "test_config.h"

/* max is already defined in windows/msvc */
#ifndef max
	static inline int max(int a, int b) {return a > b ? a : b;}
#endif

void gdSilence(int priority, const char *format, va_list args)
{
	(void)priority;
	(void)format;
	(void)args;
}

gdImagePtr gdTestImageFromPng(const char *filename)
{
	gdImagePtr image;
	FILE *fp;

	/* If the path is relative, then assume it's in the tests/ dir. */
	if (filename[0] == '/') {
		fp = fopen(filename, "rb");
		if (!fp)
			return NULL;
	} else
		fp = gdTestFileOpen(filename);

	image = gdImageCreateFromPng(fp);
	fclose(fp);
	return image;
}

static char *tmpdir_base;

/* This is kind of hacky, but it's meant to be simple. */
static void _clean_dir(const char *dir)
{
	DIR *d;
	struct dirent *de;

	d = opendir(dir);
	if (d == NULL)
		return;

	if (chdir(dir) != 0)
		goto done;

	while ((de = readdir(d)) != NULL) {
		struct stat st;

		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;

		if (lstat(de->d_name, &st) != 0)
			continue;

		if (S_ISDIR(st.st_mode))
			_clean_dir(de->d_name);
		else
			unlink(de->d_name);
	}

	if (chdir("..")) {
		/* Ignore. */;
	}

 done:
	closedir(d);
	rmdir(dir);
}

static void tmpdir_cleanup(void)
{
	_clean_dir(tmpdir_base);
	free(tmpdir_base);
}

const char *gdTestTempDir(void)
{
	if (tmpdir_base == NULL) {
		char *tmpdir, *tmpdir_root;

		tmpdir_root = getenv("TMPDIR");
		if (tmpdir_root == NULL)
			tmpdir_root = "/tmp";

		/* The constant here is a lazy over-estimate. */
		tmpdir = malloc(strlen(tmpdir_root) + 30);
		gdTestAssert(tmpdir != NULL);

		sprintf(tmpdir, "%s/gdtest.XXXXXX", tmpdir_root);

		tmpdir_base = mkdtemp(tmpdir);
		gdTestAssert(tmpdir_base != NULL);

		atexit(tmpdir_cleanup);
	}

	return tmpdir_base;
}

char *gdTestTempFile(const char *template)
{
	const char *tempdir = gdTestTempDir();
	char *ret;

	if (template == NULL)
		template = "gdtemp.XXXXXX";

	ret = malloc(strlen(tempdir) + 10 + strlen(template));
	gdTestAssert(ret != NULL);
	sprintf(ret, "%s/%s", tempdir, template);

	if (strstr(template, "XXXXXX") != NULL) {
		int fd = mkstemp(ret);
		gdTestAssert(fd != -1);
		close(fd);
	}

	return ret;
}

FILE *gdTestTempFp(void)
{
	char *file = gdTestTempFile(NULL);
	FILE *fp = fopen(file, "wb");
	gdTestAssert(fp != NULL);
	free(file);
	return fp;
}

char *gdTestFilePathV(const char *path, va_list args)
{
	size_t len;
	const char *p;
	char *file;
	va_list args_len;

	/* Figure out how much space we need. */
	va_copy(args_len, args);
	len = strlen(GDTEST_TOP_DIR) + 1;
	p = path;
	do {
		len += strlen(p) + 1;
	} while ((p = va_arg(args_len, const char *)) != NULL);
	va_end(args_len);

	/* Now build the path. */
	file = malloc(len);
	gdTestAssert(file != NULL);
	strcpy(file, GDTEST_TOP_DIR);
	p = path;
	do {
		strcat(file, "/");
		strcat(file, p);
	} while ((p = va_arg(args, const char *)) != NULL);
	va_end(args);

	return file;
}

char *gdTestFilePathX(const char *path, ...)
{
	va_list args;
	va_start(args, path);
	return gdTestFilePathV(path, args);
}

FILE *gdTestFileOpenX(const char *path, ...)
{
	va_list args;
	FILE *fp;
	char *file;

	va_start(args, path);
	file = gdTestFilePathV(path, args);
	fp = fopen(file, "rb");
	gdTestAssert(fp != NULL);
	free(file);
	return fp;
}

/* Compare two buffers, returning the number of pixels that are
 * different and the maximum difference of any single color channel in
 * result_ret.
 *
 * This function should be rewritten to compare all formats supported by
 * cairo_format_t instead of taking a mask as a parameter.
 */
void gdTestImageDiff(gdImagePtr buf_a, gdImagePtr buf_b,
                     gdImagePtr buf_diff, CuTestImageResult *result_ret)
{
	int x, y;
	int c1, c2;
#   define UP_DIFF(var) result_ret->max_diff = max((var), result_ret->max_diff)

	for (y = 0; y < gdImageSY(buf_a); y++) {
		for (x = 0; x < gdImageSX(buf_a); x++) {
			c1 = gdImageGetTrueColorPixel(buf_a, x, y);
			c2 = gdImageGetTrueColorPixel(buf_b, x, y);

			/* check if the pixels are the same */
			if (c1 != c2) {
				int r1,b1,g1,a1,r2,b2,g2,a2;
				unsigned int diff_a,diff_r,diff_g,diff_b;

				a1 = gdTrueColorGetAlpha(c1);
				a2 = gdTrueColorGetAlpha(c2);
				diff_a = abs (a1 - a2);
				diff_a *= 4;  /* emphasize */

				if (diff_a) {
					diff_a += 128; /* make sure it's visible */
				}
				if (diff_a > gdAlphaMax) {
					diff_a = gdAlphaMax/2;
				}

				r1 = gdTrueColorGetRed(c1);
				r2 = gdTrueColorGetRed(c2);
				diff_r = abs (r1 - r2);
				// diff_r *= 4;  /* emphasize */
				if (diff_r) {
					diff_r += gdRedMax/2; /* make sure it's visible */
				}
				if (diff_r > 255) {
					diff_r = 255;
				}
				UP_DIFF(diff_r);

				g1 = gdTrueColorGetGreen(c1);
				g2 = gdTrueColorGetGreen(c2);
				diff_g = abs (g1 - g2);

				diff_g *= 4;  /* emphasize */
				if (diff_g) {
					diff_g += gdGreenMax/2; /* make sure it's visible */
				}
				if (diff_g > 255) {
					diff_g = 255;
				}
				UP_DIFF(diff_g);

				b1 = gdTrueColorGetBlue(c1);
				b2 = gdTrueColorGetBlue(c2);
				diff_b = abs (b1 - b2);
				diff_b *= 4;  /* emphasize */
				if (diff_b) {
					diff_b += gdBlueMax/2; /* make sure it's visible */
				}
				if (diff_b > 255) {
					diff_b = 255;
				}
				UP_DIFF(diff_b);

				result_ret->pixels_changed++;
				if (buf_diff) gdImageSetPixel(buf_diff, x,y, gdTrueColorAlpha(diff_r, diff_g, diff_b, diff_a));
			} else {
				if (buf_diff) gdImageSetPixel(buf_diff, x,y, gdTrueColorAlpha(255,255,255,0));
			}
		}
	}
#   undef UP_DIFF
}


/* Return the largest difference between two corresponding pixels and
 * channels. */
unsigned int gdMaxPixelDiff(gdImagePtr a, gdImagePtr b)
{
    int diff = 0;
    int x, y;

    if (a == NULL || b == NULL || a->sx != b->sx || a->sy != b->sy)
        return UINT_MAX;

    for (x = 0; x < a->sx; x++) {
        for (y = 0; y < a->sy; y++) {
            int c1, c2;

			c1 = gdImageGetTrueColorPixel(a, x, y);
			c2 = gdImageGetTrueColorPixel(b, x, y);
            if (c1 == c2) continue;

            diff = max(diff, abs(gdTrueColorGetAlpha(c1) - gdTrueColorGetAlpha(c2)));
            diff = max(diff, abs(gdTrueColorGetRed(c1)   - gdTrueColorGetRed(c2)));
            diff = max(diff, abs(gdTrueColorGetGreen(c1) - gdTrueColorGetGreen(c2)));
            diff = max(diff, abs(gdTrueColorGetBlue(c1)  - gdTrueColorGetBlue(c2)));
        }/* for */
    }/* for */

    return diff;
}


int gdTestImageCompareToImage(const char* file, unsigned int line, const char* message,
                              gdImagePtr expected, gdImagePtr actual)
{
	unsigned int width_a, height_a;
	unsigned int width_b, height_b;
	gdImagePtr surface_diff = NULL;
	CuTestImageResult result = {0, 0};

	(void)message;

	if (!actual) {
		_gdTestErrorMsg(file, line, "Image is NULL\n");
		goto fail;
	}

	width_a  = gdImageSX(expected);
	height_a = gdImageSY(expected);
	width_b  = gdImageSX(actual);
	height_b = gdImageSY(actual);

	if (width_a  != width_b  || height_a != height_b) {
		_gdTestErrorMsg(file, line,
				"Image size mismatch: (%ux%u) vs. (%ux%u)\n       for %s vs. buffer\n",
				width_a, height_a,
				width_b, height_b,
				file);
		goto fail;
	}

	surface_diff = gdImageCreateTrueColor(width_a, height_a);

	gdTestImageDiff(expected, actual, surface_diff, &result);
	if (result.pixels_changed>0) {
		char file_diff[255];
		char file_out[1024];
		FILE *fp;
		int len, p;

		_gdTestErrorMsg(file, line,
				"Total pixels changed: %d with a maximum channel difference of %d.\n",
				result.pixels_changed,
				result.max_diff
			);

		p = len = strlen(file);
		p--;

		/* Use only the filename (and store it in the bld dir not the src dir
		 */
		while(p > 0 && (file[p] != '/' && file[p] != '\\')) {
			p--;
		}
		sprintf(file_diff, "%s_%u_diff.png", file + p  + 1, line);
		sprintf(file_out, "%s_%u_out.png", file + p  + 1, line);

		fp = fopen(file_diff, "wb");
		if (!fp) goto fail;
		gdImagePng(surface_diff,fp);
		fclose(fp);

		fp = fopen(file_out, "wb");
		if (!fp) goto fail;
		gdImagePng(actual, fp);
		fclose(fp);
		return 0;
	} else {
		if (surface_diff) {
			gdImageDestroy(surface_diff);
		}
		return 1;
	}

fail:
	if (surface_diff) {
		gdImageDestroy(surface_diff);
	}
	return 0;
}

int gdTestImageCompareToFile(const char* file, unsigned int line, const char* message,
                             const char *expected_file, gdImagePtr actual)
{
	gdImagePtr expected;
	int res = 1;

	expected = gdTestImageFromPng(expected_file);

	if (!expected) {
		_gdTestErrorMsg(file, line, "Cannot open PNG <%s>", expected_file);
		res = 0;
	} else {
		res = gdTestImageCompareToImage(file, line, message, expected, actual);
		gdImageDestroy(expected);
	}
	return res;
}


static int failureCount = 0;

int gdNumFailures() {
    return failureCount;
}

int _gdTestAssert(const char* file, unsigned int line, int condition)
{
	if (condition) return 1;
	_gdTestErrorMsg(file, line, "Assert failed in <%s:%i>\n", file, line);

    ++failureCount;

	return 0;
}

int _gdTestAssertMsg(const char* file, unsigned int line, int condition, const char* message, ...)
{
	va_list args;

	if (condition) return 1;

	fprintf(stderr, "%s:%u: ", file, line);
	va_start(args, message);
	vfprintf(stderr, message, args);
	va_end(args);

	fprintf(stderr, "%s:%u: %s\n", file, line, message);

	fflush(stderr);

	++failureCount;

	return 0;
}

int _gdTestErrorMsg(const char* file, unsigned int line, const char* format, ...) /* {{{ */
{
	va_list args;

	fprintf(stderr, "%s:%u: ", file, line);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fflush(stderr);

    ++failureCount;

	return 0;
}
/* }}} */
