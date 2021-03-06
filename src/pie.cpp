/*
    This file is part of WZ2100 Map Editor.
    Copyright (C) 2020-2021  maxsupermanhd
    Copyright (C) 2020-2021  bjorn-ali-goransson

    WZ2100 Map Editor is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    WZ2100 Map Editor is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with WZ2100 Map Editor; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "pie.h"
#include "log.hpp"
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

struct PIEobject ReadPIE(char* path, SDL_Renderer* rend) {
	FILE* f = fopen(path, "r");
	struct PIEobject o;
	o.valid = 0;
	if(f == NULL) {
		log_error("Error opening file");
		return o;
	}

	int type, dummy, pointscount, ret;
	char texturepagepath[512];
	ret = fscanf(f, "PIE %d\nTYPE %d\nTEXTURE %d %s %d %d\nLEVELS %d\nLEVEL %d\nPOINTS %d\n", &o.ver, &type, &dummy, texturepagepath, &dummy, &dummy, &dummy, &dummy, &pointscount);
	if(ret!=9) {
		log_error("PIE scanf 1 %d", ret);
		abort();
	}
	strncpy(o.texturepath, texturepagepath, 512);
	o.points = (struct PIEpoint*)malloc(pointscount*sizeof(struct PIEpoint));
	for(int i=0; i<pointscount; i++) {
		ret = fscanf(f, "\t%f %f %f\n", &o.points[i].x, &o.points[i].y, &o.points[i].z);
		if(ret!=3) {
			log_error("PIE scanf 2 %d", ret);
			abort();
		}
		//printf("Point %f %f %f\n", o.points[i].x, o.points[i].y, o.points[i].z);
	}
	o.pointscount = pointscount;

	int polycount;
	ret = fscanf(f, "POLYGONS %d", &polycount);
	if(ret!=1) {
		log_error("PIE scanf 3 %d", ret);
		abort();
	}
	o.polygons = (struct PIEpolygon*)malloc(polycount*sizeof(struct PIEpolygon));
	o.polygonscount = polycount;
	for(int i=0; i<polycount; i++) {
		ret = fscanf(f, "\t%d %d", &o.polygons[i].flags, &o.polygons[i].pcount);
		if(ret!=2) {
			log_error("PIE scanf 4 %d (%d)", ret, i);
			abort();
		}
		for(int j=0; j<o.polygons[i].pcount; j++) {
			ret = fscanf(f, " %d", &o.polygons[i].porder[j]);
			if(ret!=1) {
				log_error("PIE scanf 5 %d (%d) (%d)", ret, i, j);
				abort();
			}
		}
		if(o.polygons[i].flags!=200) {
			log_error("Polygons bad");
			abort();
		}
		//log_info("Tex coords:");
		for(int j=0; j<o.polygons[i].pcount*2; j++) {
			ret = fscanf(f, " %f", &o.polygons[i].texcoords[j]);
			//log_info("%f", o.polygons[i].texcoords[j]);
			if(ret!=1) {
				log_error("PIE scanf 6 %d (%d) (%d)\n", ret, i, j);
				abort();
			}
		}
	}
	fclose(f);
	o.valid = 1;
	return o;
}

void PIEprepareGLarrays(PIEobject* o) {
	int w, h;
	SDL_QueryTexture(o->texture, NULL, NULL, &w, &h);

	size_t pfillmax = 0;
	for(int i=0; i<o->polygonscount; i++) {
		pfillmax += o->polygons[i].pcount * (3 + 2);
	}
	o->GLvertexes = (float*)malloc(pfillmax*sizeof(float));
	o->GLvertexesCount = pfillmax*sizeof(float);
	size_t pfillc = 0;
	for(int i=0; i<o->polygonscount; i++) {
		if(o->polygons[i].pcount != 3) {
			log_fatal("Polygon converter error!");
			abort();
		}
		for(int j=0; j<o->polygons[i].pcount; j++) {
			o->GLvertexes[pfillc+0] = o->points[o->polygons[i].porder[j]].x;
			o->GLvertexes[pfillc+1] = o->points[o->polygons[i].porder[j]].y;
			o->GLvertexes[pfillc+2] = o->points[o->polygons[i].porder[j]].z;
			o->GLvertexes[pfillc+3] = (o->polygons[i].texcoords[j*2+0]*4)/w;
			o->GLvertexes[pfillc+4] = (o->polygons[i].texcoords[j*2+1]*4)/h;
			pfillc+=5;
		}
	}
}

bool PIEreadTexture(PIEobject* o, SDL_Renderer* rend) {
	char fbufer[2048] = {0};
	snprintf(fbufer, 2047, "./%s", o->texturepath);
	log_info("Loading [%s] texpage...", o->texturepath);
	SDL_Surface* loadedSurf = IMG_Load(fbufer);
	if(loadedSurf==NULL) {
		log_fatal("Texture Loading error: %s\n", IMG_GetError());
		return false;
	} else {
		o->texture = SDL_CreateTextureFromSurface(rend, loadedSurf);
		if(o->texture == NULL) {
			log_fatal("Texture converting error: %s\n", IMG_GetError());
			return false;
		}
		SDL_FreeSurface(loadedSurf);
	}
	SDL_QueryTexture(o->texture, NULL, NULL, &o->texturewidth, &o->textureheight);
	log_info("Loaded [%s] texpage.", o->texturepath);
	return true;
}

void PIEbindTexpage(PIEobject* o) {
	float texw, texh;
	SDL_GL_BindTexture(o->texture, &texw, &texh);
	log_info("Texture binded: %f %f", texw, texh);
}

void FreePIE(struct PIEobject* o) {
	if(o->valid) {
		free(o->polygons);
		o->polygons = NULL;
		free(o->points);
		o->points = NULL;
		free(o->GLvertexes);
		o->GLvertexes = NULL;
	}
	return;
}
