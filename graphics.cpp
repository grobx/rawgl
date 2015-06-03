
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "graphics.h"
#include "util.h"


Graphics::Graphics() {
	memset(_pagePtrs, 0, sizeof(_pagePtrs));
	setSize(GFX_W, GFX_H);
}

Graphics::~Graphics() {
	for (int i = 0; i < 4; ++i) {
		free(_pagePtrs[i]);
		_pagePtrs[i] = 0;
	}
}

void Graphics::setSize(int w, int h) {
	_u = (w << 16) / GFX_W;
	_v = (h << 16) / GFX_H;
	_w = w;
	_h = h;
	for (int i = 0; i < 4; ++i) {
		_pagePtrs[i] = (uint8_t *)realloc(_pagePtrs[i], _w * _h);
		if (!_pagePtrs[i]) {
			error("Not enough memory to allocate offscreen buffers");
		}
		memset(_pagePtrs[i], 0, _w * _h);
	}
	setWorkPagePtr(2);
}

static uint32_t calcStep(const Point &p1, const Point &p2, uint16_t &dy) {
	dy = p2.y - p1.y;
	uint16_t delta = (dy == 0) ? 1 : dy;
	return ((p2.x - p1.x) << 16) / delta;
}

void Graphics::drawPolygon(uint8_t color, const QuadStrip &quadStrip) {
	QuadStrip qs = quadStrip;
	if (_w != GFX_W || _h != GFX_H) {	
		for (int i = 0; i < qs.numVertices; ++i) {
			qs.vertices[i].scale(_u, _v);
		}
	}

	int i = 0;
	int j = qs.numVertices - 1;

	int16_t x2 = qs.vertices[i].x;
	int16_t x1 = qs.vertices[j].x;
	int16_t hliney = MIN(qs.vertices[i].y, qs.vertices[j].y);

	++i;
	--j;

	drawLine pdl;
	switch (color) {
	default:
		pdl = &Graphics::drawLineN;
		break;
	case 0x11:
		pdl = &Graphics::drawLineP;
		break;
	case 0x10:
		pdl = &Graphics::drawLineT;
		break;
	}

	uint32_t cpt1 = x1 << 16;
	uint32_t cpt2 = x2 << 16;

	int numVertices = qs.numVertices;
	while (1) {
		numVertices -= 2;
		if (numVertices == 0) {
			return;
		}
		uint16_t h;
		uint32_t step1 = calcStep(qs.vertices[j + 1], qs.vertices[j], h);
		uint32_t step2 = calcStep(qs.vertices[i - 1], qs.vertices[i], h);
		
		++i;
		--j;

		cpt1 = (cpt1 & 0xFFFF0000) | 0x7FFF;
		cpt2 = (cpt2 & 0xFFFF0000) | 0x8000;

		if (h == 0) {
			cpt1 += step1;
			cpt2 += step2;
		} else {
			while (h--) {
				if (hliney >= 0) {
					x1 = cpt1 >> 16;
					x2 = cpt2 >> 16;
					if (x1 < _w && x2 >= 0) {
						if (x1 < 0) x1 = 0;
						if (x2 >= _w) x2 = _w - 1;
						(this->*pdl)(x1, x2, hliney, color);
					}
				}
				cpt1 += step1;
				cpt2 += step2;
				++hliney;
				if (hliney >= _h) return;
			}
		}
	}
}

void Graphics::drawChar(uint8_t c, uint16_t x, uint16_t y, uint8_t color) {
	if (x <= 39 && y <= 192) {
		const uint8_t *ft = _font + (c - 0x20) * 8;
		uint8_t *p = _workPagePtr + x * 8 + y * _w;
		for (int j = 0; j < 8; ++j) {
			uint8_t ch = *(ft + j);
			for (int i = 0; i < 8; ++i) {
				if (ch & 0x80) {
					p[j * _w + i] = color;
				}
				ch <<= 1;
			}
		}
	}
}

void Graphics::drawPoint(int16_t x, int16_t y, uint8_t color) {
	const int off = y * _w + x;
	switch (color) {
	case 0x10:
		_workPagePtr[off] |= 0x8;
		break;
	case 0x11:
		_workPagePtr[off] = *(_pagePtrs[0] + off);
		break;
	default:
		_workPagePtr[off] = color;
		break;
	}
}

void Graphics::drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	int w = xmax - xmin + 1;
	uint8_t *p = _workPagePtr + y * _w + xmin;
	while (w--) {
		*p++ |= 0x8;
	}
}

void Graphics::drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int off = y * _w + xmin;
	memset(_workPagePtr + off, color, w);
}

void Graphics::drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	if (_workPagePtr == _pagePtrs[0]) {
		return;
	}
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int off = y * _w + xmin;
	memcpy(_workPagePtr + off, _pagePtrs[0] + off, w);
}

uint8_t *Graphics::getPagePtr(uint8_t page) {
	assert(page >= 0 && page < 4);
	return _pagePtrs[page];
}

void Graphics::setWorkPagePtr(uint8_t page) {
	_workPagePtr = getPagePtr(page);
}
