/* tab:4
 *
 * photo.c - photo display functions
 *
 * "Copyright (c) 2011 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:        Steve Lumetta
 * Version:       3
 * Creation Date: Fri Sep 9 21:44:10 2011
 * Filename:      photo.c
 * History:
 *    SL    1    Fri Sep 9 21:44:10 2011
 *        First written(based on mazegame code).
 *    SL    2    Sun Sep 11 14:57:59 2011
 *        Completed initial implementation of functions.
 *    SL    3    Wed Sep 14 21:49:44 2011
 *        Cleaned up code for distribution.
 */


#include <string.h>

#include "assert.h"
#include "modex.h"
#include "photo.h"
#include "photo_headers.h"
#include "world.h"


/* types local to this file(declared in types.h) */

/*
 * A room photo.  Note that you must write the code that selects the
 * optimized palette colors and fills in the pixel data using them as
 * well as the code that sets up the VGA to make use of these colors.
 * Pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of
 * the second row, and so forth.  No padding should be used.
 */
struct photo_t {
    photo_header_t hdr;            /* defines height and width */
    uint8_t        palette[192][3];     /* optimized palette colors */
    uint8_t*       img;                 /* pixel data               */
};

/*
 * An object image.  The code for managing these images has been given
 * to you.  The data are simply loaded from a file, where they have
 * been stored as 2:2:2-bit RGB values(one byte each), including
 * transparent pixels(value OBJ_CLR_TRANSP).  As with the room photos,
 * pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of the
 * second row, and so forth.  No padding is used.
 */
struct image_t {
    photo_header_t hdr;  /* defines height and width */
    uint8_t*       img;  /* pixel data               */
};


/* file-scope variables */

/*the struction of octree node*/
struct octree_node {
	uint16_t idx_by_rgb;
	uint16_t level2_idx;
	uint16_t palette_idx;
	unsigned long int red_sum;
	unsigned long int green_sum;
	unsigned long int blue_sum;
	unsigned int pixel_number;
};


/*
 * The room currently shown on the screen.  This value is not known to
 * the mode X code, but is needed when filling buffers in callbacks from
 * that code(fill_horiz_buffer/fill_vert_buffer).  The value is set
 * by calling prep_room.
 */
static const room_t* cur_room = NULL;


/*
 * fill_horiz_buffer
 *   DESCRIPTION: Given the(x,y) map pixel coordinate of the leftmost
 *                pixel of a line to be drawn on the screen, this routine
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS:(x,y) -- leftmost pixel of line to be drawn
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_horiz_buffer(int x, int y, unsigned char buf[SCROLL_X_DIM]) {
    int            idx;   /* loop index over pixels in the line          */
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgx;  /* loop index over pixels in object image      */
    int            yoff;  /* y offset into object image                  */
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo(cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_X_DIM; idx++) {
        buf[idx] = (0 <= x + idx && view->hdr.width > x + idx ? view->img[view->hdr.width * y + x + idx] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate(cur_room); NULL != obj; obj = obj_next(obj)) {
        obj_x = obj_get_x(obj);
        obj_y = obj_get_y(obj);
        img = obj_image(obj);

        /* Is object outside of the line we're drawing? */
        if (y < obj_y || y >= obj_y + img->hdr.height || x + SCROLL_X_DIM <= obj_x || x >= obj_x + img->hdr.width) {
            continue;
        }

        /* The y offset of drawing is fixed. */
        yoff = (y - obj_y) * img->hdr.width;

        /*
         * The x offsets depend on whether the object starts to the left
         * or to the right of the starting point for the line being drawn.
         */
        if (x <= obj_x) {
            idx = obj_x - x;
            imgx = 0;
        }
        else {
            idx = 0;
            imgx = x - obj_x;
        }

        /* Copy the object's pixel data. */
        for (; SCROLL_X_DIM > idx && img->hdr.width > imgx; idx++, imgx++) {
            pixel = img->img[yoff + imgx];

            /* Don't copy transparent pixels. */
            if (OBJ_CLR_TRANSP != pixel) {
                buf[idx] = pixel;
            }
        }
    }
}


/*
 * fill_vert_buffer
 *   DESCRIPTION: Given the(x,y) map pixel coordinate of the top pixel of
 *                a vertical line to be drawn on the screen, this routine
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS:(x,y) -- top pixel of line to be drawn
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_vert_buffer(int x, int y, unsigned char buf[SCROLL_Y_DIM]) {
    int            idx;   /* loop index over pixels in the line          */
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgy;  /* loop index over pixels in object image      */
    int            xoff;  /* x offset into object image                  */
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo(cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_Y_DIM; idx++) {
        buf[idx] = (0 <= y + idx && view->hdr.height > y + idx ? view->img[view->hdr.width *(y + idx) + x] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate(cur_room); NULL != obj; obj = obj_next(obj)) {
        obj_x = obj_get_x(obj);
        obj_y = obj_get_y(obj);
        img = obj_image(obj);

        /* Is object outside of the line we're drawing? */
        if (x < obj_x || x >= obj_x + img->hdr.width ||
            y + SCROLL_Y_DIM <= obj_y || y >= obj_y + img->hdr.height) {
            continue;
        }

        /* The x offset of drawing is fixed. */
        xoff = x - obj_x;

        /*
         * The y offsets depend on whether the object starts below or
         * above the starting point for the line being drawn.
         */
        if (y <= obj_y) {
            idx = obj_y - y;
            imgy = 0;
        }
        else {
            idx = 0;
            imgy = y - obj_y;
        }

        /* Copy the object's pixel data. */
        for (; SCROLL_Y_DIM > idx && img->hdr.height > imgy; idx++, imgy++) {
            pixel = img->img[xoff + img->hdr.width * imgy];

            /* Don't copy transparent pixels. */
            if (OBJ_CLR_TRANSP != pixel) {
                buf[idx] = pixel;
            }
        }
    }
}


/*
 * image_height
 *   DESCRIPTION: Get height of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t image_height(const image_t* im) {
    return im->hdr.height;
}


/*
 * image_width
 *   DESCRIPTION: Get width of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t image_width(const image_t* im) {
    return im->hdr.width;
}

/*
 * photo_height
 *   DESCRIPTION: Get height of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t photo_height(const photo_t* p) {
    return p->hdr.height;
}


/*
 * photo_width
 *   DESCRIPTION: Get width of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t photo_width(const photo_t* p) {
    return p->hdr.width;
}


/*
 * prep_room
 *   DESCRIPTION: Prepare a new room for display.  You might want to set
 *                up the VGA palette registers according to the color
 *                palette that you chose for this room.
 *   INPUTS: r -- pointer to the new room
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes recorded cur_room for this file
 */
void prep_room(const room_t* r) {
    /* Record the current room. */
	photo_t *p = room_photo(r);
	fill_palette(p->palette);
    cur_room = r;
}


/*
 * read_obj_image
 *   DESCRIPTION: Read size and pixel data in 2:2:2 RGB format from a
 *                photo file and create an image structure from it.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the image
 */
image_t* read_obj_image(const char* fname) {
    FILE*    in;        /* input file               */
    image_t* img = NULL;    /* image structure          */
    uint16_t x;            /* index over image columns */
    uint16_t y;            /* index over image rows    */
    uint8_t  pixel;        /* one pixel from the file  */

    /*
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the image pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen(fname, "r+b")) ||
        NULL == (img = malloc(sizeof (*img))) ||
        NULL != (img->img = NULL) || /* false clause for initialization */
        1 != fread(&img->hdr, sizeof (img->hdr), 1, in) ||
        MAX_OBJECT_WIDTH < img->hdr.width ||
        MAX_OBJECT_HEIGHT < img->hdr.height ||
        NULL == (img->img = malloc
        (img->hdr.width * img->hdr.height * sizeof (img->img[0])))) {
        if (NULL != img) {
            if (NULL != img->img) {
                free(img->img);
            }
            free(img);
        }
        if (NULL != in) {
            (void)fclose(in);
        }
        return NULL;
    }

    /*
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order(top to bottom).
     */
    for (y = img->hdr.height; y-- > 0; ) {

        /* Loop over columns from left to right. */
        for (x = 0; img->hdr.width > x; x++) {

            /*
             * Try to read one 8-bit pixel.  On failure, clean up and
             * return NULL.
             */
            if (1 != fread(&pixel, sizeof (pixel), 1, in)) {
                free(img->img);
                free(img);
                (void)fclose(in);
                return NULL;
            }

            /* Store the pixel in the image data. */
            img->img[img->hdr.width * y + x] = pixel;
        }
    }

    /* All done.  Return success. */
    (void)fclose(in);
    return img;
}


/*
 * read_photo
 *   DESCRIPTION: Read size and pixel data in 5:6:5 RGB format from a
 *                photo file and create a photo structure from it.
 *                Code provided simply maps to 2:2:2 RGB.  You must
 *                replace this code with palette color selection, and
 *                must map the image pixels into the palette colors that
 *                you have defined.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the photo
 */
photo_t* read_photo(const char* fname) {
    FILE*    in;    /* input file               */
    photo_t* p = NULL;    /* photo structure          */
    uint16_t x;        /* index over image columns */
    uint16_t y;        /* index over image rows    */
    uint16_t pixel;    /* one pixel from the file  */

    /*
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the photo pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen(fname, "r+b")) ||
        NULL == (p = malloc(sizeof (*p))) ||
        NULL != (p->img = NULL) || /* false clause for initialization */
        1 != fread(&p->hdr, sizeof (p->hdr), 1, in) ||
        MAX_PHOTO_WIDTH < p->hdr.width ||
        MAX_PHOTO_HEIGHT < p->hdr.height ||
        NULL == (p->img = malloc
        (p->hdr.width * p->hdr.height * sizeof (p->img[0])))) {
        if (NULL != p) {
            if (NULL != p->img) {
                free(p->img);
            }
            free(p);
        }
        if (NULL != in) {
            (void)fclose(in);
        }
        return NULL;
    }
	
	struct octree_node level4[LEVEL4_NODE_NUMBER]; 										//the nodes for level4
	struct octree_node level2[LEVEL2_NODE_NUMBER];										//the nodes for level2
	
	int level4_new_index[LEVEL4_NODE_NUMBER];
	uint32_t image_size;																//
	uint32_t i;																			// for index
	uint16_t pixels_data[p->hdr.height * p->hdr.width];
	uint32_t r_average, g_average, b_average;
	int index; 
	
	image_size = p->hdr.height * p->hdr.width;
	//initial level 4:
	for(i=0; i<LEVEL4_NODE_NUMBER; i++){
		level4[i].idx_by_rgb = i;
		level4[i].level2_idx = 65;						//because level2 has only 64 bits
		level4[i].palette_idx = -1;
		level4[i].red_sum = 0;
		level4[i].green_sum = 0;
		level4[i].blue_sum = 0;
		level4[i].pixel_number = 0;
		level4_new_index[i] = -1;
	}

	//initial level 2:				level2_idx and palette_idx for level2 will not be used
	for(i=0; i<LEVEL2_NODE_NUMBER; i++){
		level2[i].idx_by_rgb = i;
		level2[i].level2_idx = 65;						//because level2 has only 64 bits
		level2[i].palette_idx = -1;
		level2[i].red_sum = 0;
		level2[i].green_sum = 0;
		level2[i].blue_sum = 0;
		level2[i].pixel_number = 0;
	}
		
    /*
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order(top to bottom).
     */
    for (y = p->hdr.height; y-- > 0; ) {

        /* Loop over columns from left to right. */
        for (x = 0; p->hdr.width > x; x++) {

            /*
             * Try to read one 16-bit pixel.  On failure, clean up and
             * return NULL.
             */
            if (1 != fread(&pixel, sizeof (pixel), 1, in)) {
                free(p->img);
                free(p);
                (void)fclose(in);
                return NULL;
            }
            /*
             * 16-bit pixel is coded as 5:6:5 RGB(5 bits red, 6 bits green,
             * and 6 bits blue).  We change to 2:2:2, which we've set for the
             * game objects.  You need to use the other 192 palette colors
             * to specialize the appearance of each photo.
             *
             * In this code, you need to calculate the p->palette values,
             * which encode 6-bit RGB as arrays of three uint8_t's.  When
             * the game puts up a photo, you should then change the palette
             * to match the colors needed for that photo.
             */

			index = idx_in_level(pixel, 4);
			 
			level4[index].pixel_number++;
			level4[index].red_sum += (pixel >> 11) & 0x001F;		//// 0x1F because just need the last 5 bits
			level4[index].green_sum += (pixel >> 5) & 0x003F;		// 0x3F because just need the last 6 bits
			level4[index].blue_sum += (pixel) & 0x001F;				// 0x1F because just need the last 5 bits
			level4[index].level2_idx = idx_in_level(pixel, 2);
			level4[index].idx_by_rgb = index;
			 
			 
			index = idx_in_level(pixel, 2);
			level2[index].pixel_number++;
			level2[index].red_sum += (pixel >> 11) & 0x001F;		//// 0x1F because just need the last 5 bits
			level2[index].green_sum += (pixel >> 5) & 0x003F;		// 0x3F because just need the last 6 bits
			level2[index].blue_sum += (pixel) & 0x001F;				// 0x1F because just need the last 5 bits			 

			pixels_data[(y * p->hdr.width) + x] = pixel; 
        }
    }

    /* All done.  Return success. */
    (void)fclose(in);
    
	qsort(level4, LEVEL4_NODE_NUMBER, sizeof(struct octree_node), compar);
			
	
	for(i=0; i<LEVEL4_NODE_USED; i++){
		if(level4[i].pixel_number != 0){
			r_average = level4[i].red_sum / level4[i].pixel_number;
			g_average = level4[i].green_sum / level4[i].pixel_number;
			b_average = level4[i].blue_sum / level4[i].pixel_number;
		}else{
			r_average = 0;
			g_average = 0;
			b_average = 0;
		}
		p->palette[i][0] = (uint8_t)(r_average & 0x1F) << 1;			//just need the last 5 bits
		p->palette[i][1] = (uint8_t)(g_average & 0x3F);					//just need the last 6 bits
		p->palette[i][2] = (uint8_t)(b_average & 0x1F) << 1;			//just need the last 5 bits
		
		level4[i].palette_idx = 64 + i;						//because there are 64 palette that has been used
		level4_new_index[level4[i].idx_by_rgb] = i;
	}
	
	for(i=0; i < LEVEL2_NODE_NUMBER; i++){
		if(level2[i].pixel_number != 0){
			r_average = level2[i].red_sum / level2[i].pixel_number;
			g_average = level2[i].green_sum / level2[i].pixel_number;
			b_average = level2[i].blue_sum / level2[i].pixel_number;
		}else{
			r_average = 0;
			g_average = 0;
			b_average = 0;
		}
		//level4_new_index[level4[i].idx_by_rgb] = i;
		p->palette[i + LEVEL4_NODE_USED][0] = (uint8_t)(r_average & 0x1F) << 1;		//just need the last 5 bits
		p->palette[i + LEVEL4_NODE_USED][1] = (uint8_t)(g_average & 0x3F);			//just need the last 6 bits
		p->palette[i + LEVEL4_NODE_USED][2] = (uint8_t)(b_average & 0x1F) << 1;		//just need the last 5 bits
		
		level2[i].palette_idx = 64 + LEVEL4_NODE_USED + i;    //64 because there are 64 palette that has been used
	}
	for(i = LEVEL4_NODE_USED; i < LEVEL4_NODE_NUMBER; i++){
		level4_new_index[level4[i].idx_by_rgb] = i;
		if(level4[i].level2_idx < 64){	//64 is the index of level2
			level4[i].palette_idx = level2[level4[i].level2_idx].palette_idx;
		}
	}
	for(i = 0; i<image_size; i++){
		index = level4_new_index[idx_in_level(pixels_data[i],4)];
		p->img[i] = level4[index].palette_idx;
		
	}
	
	return p;
}


/*
 * idx_in_level
 *   DESCRIPTION: find the index in level 2 or level 4

 *   INPUTS: pixel : pixel that need to find index
 *			  k: level 2 or 4
 *   OUTPUTS: the index in level 2 or level 4
 *   
 *   
 *   SIDE EFFECTS: none
 */

uint16_t idx_in_level(uint16_t pixel, int k){
	if(k == 2){			//RRRRRGGGGGGBBBBB -> 0000000000RRGGBB
		return  (((pixel >> 14) << 4) | (((pixel >> 9) & 0x3) << 2) | ((pixel >> 3) & 0x3));		//0x3 because only need the last 2 bits
	}
	if(k == 4){				//RRRRRGGGGGGBBBBB -> 0000RRRRGGGGBBBB
		return (((pixel >> 12) << 8) | (((pixel >> 7) & 0xF) << 4) | ((pixel >> 1) & 0xF));		//0x3 because only need the last 2 bits
	}
	else {
		return -1;
	}
}
	
/*
 * compar
 *   DESCRIPTION: determine the order of the node 

 *   INPUTS: p1 --first node to be compared
 *			 p2 --second node to be compared
 *   OUTPUTS: none
 *   RETURN VALUE: -1 if p1 is before p2
 *					0 if the order is not sure
 *					1 if p1 is after p2
 *   SIDE EFFECTS: none
 */
	
int compar(const void *p1, const void *p2){
	const struct octree_node* node_1 = p1;
	const struct octree_node* node_2 = p2;
	if(node_2->pixel_number < node_1->pixel_number)  return -1;
	if(node_2->pixel_number > node_1->pixel_number)  return 1;
	else{return 0;}
}

	
