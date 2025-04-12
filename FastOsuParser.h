/*
Copyright (c) 2025 Amar Alic

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _FAST_OSU_PARSER_H
#define _FAST_OSU_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



typedef struct {
        // [General]:
        char audio_file_name[256];
        short audio_file_name_size;
        int audio_lead_in;
        int countdown; // 0 = None, 1 = Normal, 2 = Half, 3 = Double
        float stack_leniency;
        int mode; // 0 = std, 1 = taiko, 2 = fruits, 3 = mania
        int countdown_offset;

        // [Metadata]:
        char title[256];
        size_t title_size;
        char artist[256];
        size_t artist_size;
        char creator[256];
        size_t creator_size;
        char version[256]; // diff name
        size_t version_size;
        int beatmap_id; // diff ID
        int beatmap_set_id; // beatmap ID

        // [Difficulty]:
        float hp_drain_rate;
        float circle_size;
        float overall_difficulty;
        float approach_rate;
        double slider_multiplier;
        double slider_tick_rate;

        // [TimingPoints]:
        struct {
                int time;
                double beat_length;
                int meter;
                char b_uninherited;
        }* timing_points;
        size_t timing_points_count;

        // [HitObjects]:
        struct {
                int x;
                int y;
                int time;
                int type;
                union {
                        // slider
                        struct {
                                char curve_type; // "B", "C", "L", or "P"
                                struct {
                                        int x;
                                        int y;
                                }* curve_points;
                                size_t curve_points_count;
                                int slides;
                                double length;
                        };
                        
                        // spinner
                        int end_time;
                } object_params;
        }* hit_objects;
        size_t hit_objects_count;

        void** _total_curve_points;
        size_t _total_curve_points_count;

} FastOsuParser__Beatmap;



typedef enum {
        FastOsuParser__SUCCESS,
        FastOsuParser__ERROR_FAILED_TO_OPEN_FILE,
        FastOsuParser__ERROR_FAILED_TO_SEEK_FILE_END,
        FastOsuParser__ERROR_FAILED_TO_TELL_FILE,
        FastOsuParser__ERROR_FAILED_TO_SEEK_FILE_START,
        FastOsuParser__ERROR_FAILED_TO_ALLOCATE_MEMORY,
        FastOsuParser__ERROR_FAILED_TO_READ_FILE,
        FastOsuParser__ERROR_FAILED_TO_CLOSE_FILE,
        FastOsuParser__ERROR_SECTION_GENERAL_AUDIOFILENAME_TOO_LONG,
        FastOsuParser__ERROR_SECTION_METADATA_TITLE_TOO_LONG,
        FastOsuParser__ERROR_SECTION_METADATA_ARTIST_TOO_LONG,
        FastOsuParser__ERROR_SECTION_METADATA_CREATOR_TOO_LONG,
        FastOsuParser__ERROR_SECTION_METADATA_VERSION_TOO_LONG
} FastOsuParser__Error;

// Make sure "*out" is 0-initialized
FastOsuParser__Error FastOsuParser__Parse(char* path, FastOsuParser__Beatmap* out) {

        out->_total_curve_points = malloc(0); // For freeing dynamic memory in FastOsuParser__Free()



        FILE* beatmap_file = fopen(path, "rb"); //
        if (beatmap_file == NULL) return FastOsuParser__ERROR_FAILED_TO_OPEN_FILE;

        if (fseek(beatmap_file, 0, SEEK_END) != 0) return FastOsuParser__ERROR_FAILED_TO_SEEK_FILE_END;
        long beatmap_file_size = ftell(beatmap_file); //
        if (beatmap_file_size == -1L) return FastOsuParser__ERROR_FAILED_TO_TELL_FILE;
        if (fseek(beatmap_file, 0, SEEK_SET) != 0) return FastOsuParser__ERROR_FAILED_TO_SEEK_FILE_START;

        char* beatmap_file_contents = malloc(beatmap_file_size); //
        if (beatmap_file_contents == NULL) return FastOsuParser__ERROR_FAILED_TO_ALLOCATE_MEMORY;
        if (fread(beatmap_file_contents, 1, beatmap_file_size, beatmap_file) < beatmap_file_size) return FastOsuParser__ERROR_FAILED_TO_READ_FILE;

        if (fclose(beatmap_file) != 0) return FastOsuParser__ERROR_FAILED_TO_CLOSE_FILE;



        enum {
                SECTION_NONE, // *none of the desired sections
                SECTION_GENERAL,
                SECTION_METADATA,
                SECTION_DIFFICULTY,
                SECTION_TIMING_POINTS,
                SECTION_HIT_OBJECTS
        } current_section = SECTION_NONE;

        char* i = beatmap_file_contents; // Current beatmap file content index
        while (*i != '[') i++; // Skip to first section
        for (; i < beatmap_file_contents+beatmap_file_size; i++) {

                if (*i == '[') { // If new section:

                        i++;
                        switch (*i) { // Update current_section state based on first letter

                                case 'G':
                                        current_section = SECTION_GENERAL;
                                        i += sizeof("eneral]\r\n"); // Skip to next line
                                break;

                                case 'M':
                                        current_section = SECTION_METADATA;
                                        i += sizeof("etadata]\r\n"); // Skip to next line
                                break;

                                case 'D':
                                        current_section = SECTION_DIFFICULTY;
                                        i += sizeof("ifficulty]\r\n"); // Skip to next line
                                break;

                                case 'T':
                                        current_section = SECTION_TIMING_POINTS;
                                        i += sizeof("imingPoints]\r\n"); // Skip to next line
                                break;

                                case 'H':
                                        current_section = SECTION_HIT_OBJECTS;
                                        i += sizeof("itObjects]\r\n"); // Skip to next line
                                break;

                                default: current_section = SECTION_NONE;

                        }

                }



                switch (current_section) {

                        case SECTION_NONE: break;



                        case SECTION_GENERAL:

                                switch (*i) {

                                        case 'A': // [A]udio...

                                                i += sizeof("udio"); // Skip to next unique letter
                                                switch (*i) { // Check next unique letter ("Audio[X]...")

                                                        case 'F': // Audio[F]ilename
                                                        {

                                                                i += sizeof("ilename: "); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;
                                                                if (content_size > 255) return FastOsuParser__ERROR_SECTION_GENERAL_AUDIOFILENAME_TOO_LONG;

                                                                memcpy(&out->audio_file_name, i, content_size);
                                                                out->audio_file_name_size = content_size;

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                        case 'L': // Audio[L]eadIn
                                                        {

                                                                i += sizeof("eadIn: "); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;

                                                                out->audio_lead_in = atoi(i);

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                }

                                        break;

                                        case 'C': // [C]ountdown[...]

                                                i += sizeof("ountdown"); // Move to next unique character
                                                switch (*i) { // Check next unique character

                                                        case ':': // Countdown[:]
                                                        {

                                                                i += sizeof(" "); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;

                                                                out->countdown = atoi(i);

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                        case 'O': // Countdown[O]ffset
                                                        {

                                                                i += sizeof("ffset: "); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;

                                                                out->countdown_offset = atoi(i);

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                }

                                        break;

                                        case 'S':

                                                i += 7; // Skip to next unique letter
                                                switch (*i) {

                                                        case 'n': // StackLe[n]iency
                                                        {

                                                                i += sizeof("iency: "); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;

                                                                out->stack_leniency = atof(i);

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                }

                                        break;

                                        case 'M': // [M]ode
                                        {

                                                i += sizeof ("ode: "); // Skip to content

                                                out->mode = atoi(i); // (single-digit)

                                                i += 2; // Skip to next line

                                        }
                                        break;

                                }
                        
                        break;



                        case SECTION_METADATA:

                                switch (*i) {

                                        case 'T':

                                                i++; // Skip to next unique* letter
                                                switch (*i) {

                                                        case 'i': // T[i]tle?
                                                        {

                                                                if (*(i+sizeof("tle")) == 'U') break; // Skip if "TitleUnicode"

                                                                i += sizeof("tle:"); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;
                                                                if (content_size > 255) return FastOsuParser__ERROR_SECTION_METADATA_TITLE_TOO_LONG;

                                                                memcpy(&out->title, i, content_size);
                                                                out->title_size = content_size;

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                }

                                        break;

                                        case 'A': // [A]rtist?
                                        {

                                                if (*(i+sizeof("rtist")) == 'U') break; // Skip if "ArtistUnicode"

                                                i += sizeof("rtist:"); // Skip to content

                                                size_t content_size = 0;
                                                while (*(i+content_size) != '\r') content_size++;
                                                if (content_size > 255) return FastOsuParser__ERROR_SECTION_METADATA_ARTIST_TOO_LONG;

                                                memcpy(&out->artist, i, content_size);
                                                out->artist_size = content_size;

                                                i += content_size+1; // Skip to next line

                                        }
                                        break;

                                        case 'C': // [C]reator
                                        {

                                                i += sizeof("reator:"); // Skip to content

                                                size_t content_size = 0;
                                                while (*(i+content_size) != '\r') content_size++;
                                                if (content_size > 255) return FastOsuParser__ERROR_SECTION_METADATA_CREATOR_TOO_LONG;

                                                memcpy(&out->creator, i, content_size);
                                                out->creator_size = content_size;

                                                i += content_size+1; // Skip to next line

                                        }
                                        break;

                                        case 'V': // [V]ersion
                                        {

                                                i += sizeof("ersion:"); // Skip to content

                                                size_t content_size = 0;
                                                while (*(i+content_size) != '\r') content_size++;
                                                if (content_size > 255) return FastOsuParser__ERROR_SECTION_METADATA_VERSION_TOO_LONG;

                                                memcpy(&out->version, i, content_size);
                                                out->version_size = content_size;

                                                i += content_size+1; // Skip to next line

                                        }
                                        break;

                                        case 'B': // [B]eatmap...

                                                i += sizeof("eatmap"); // Skip to next unique letter
                                                switch (*i) { // Check next unique letter (Beatmap[X]...)

                                                        case 'I': // Beatmap[I]D
                                                        {

                                                                i += sizeof("D:"); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;

                                                                out->beatmap_id = atoi(i);

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                        case 'S': // Beatmap[S]etID
                                                        {

                                                                i += sizeof("etID:"); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;

                                                                out->beatmap_set_id = atoi(i);

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                }

                                        break;

                                }

                        break;



                        case SECTION_DIFFICULTY:

                                switch (*i) {

                                        case 'H': // [H]PDrainRate
                                        {

                                                i += sizeof("PDrainRate:"); // Skip to content

                                                size_t content_size = 0;
                                                while (*(i+content_size) != '\r') content_size++;

                                                out->hp_drain_rate = atof(i);

                                                i += content_size+1; // Skip to next line

                                        }
                                        break;

                                        case 'C': // [C]ircleSize
                                        {

                                                i += sizeof("ircleSize:"); // Skip to content

                                                size_t content_size = 0;
                                                while (*(i+content_size) != '\r') content_size++;

                                                out->circle_size = atof(i);

                                                i += content_size+1; // Skip to next line

                                        }
                                        break;

                                        case 'O': // [O]verallDifficulty
                                        {

                                                i += sizeof("verallDifficulty:"); // Skip to content

                                                size_t content_size = 0;
                                                while (*(i+content_size) != '\r') content_size++;

                                                out->overall_difficulty = atof(i);

                                                i += content_size+1; // Skip to next line

                                        }
                                        break;

                                        case 'A': // [A]pproachRate
                                        {

                                                i += sizeof("pproachRate:"); // Skip to content

                                                size_t content_size = 0;
                                                while (*(i+content_size) != '\r') content_size++;

                                                out->approach_rate = atof(i);

                                                i += content_size+1; // Skip to next line

                                        }
                                        break;

                                        case 'S': // [S]lider...?

                                                i += sizeof("lider"); // Skip to next unique letter
                                                switch (*i) {

                                                        case 'M': // Slider[M]ultiplier
                                                        {

                                                                i += sizeof("ultiplier:"); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;

                                                                out->slider_multiplier = atof(i);

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                        case 'T': // Slider[T]ickRate
                                                        {

                                                                i += sizeof("ickRate:"); // Skip to content

                                                                size_t content_size = 0;
                                                                while (*(i+content_size) != '\r') content_size++;

                                                                out->slider_tick_rate = atof(i);

                                                                i += content_size+1; // Skip to next line

                                                        }
                                                        break;

                                                }

                                        break;

                                }

                        break;



                        case SECTION_TIMING_POINTS:

                                size_t timing_points_count = 0;
                                for ( // Loop until "\n\r" pattern (two new lines back-to-back)
                                        int line_offset = 0;
                                        *(i+line_offset) != '\r';
                                        line_offset++
                                ) {
                                        while (*(i+line_offset) != '\n') line_offset++;
                                        timing_points_count++;
                                }

                                out->timing_points = malloc(sizeof(*(out->timing_points)) * timing_points_count);

                                for (int tp = 0; tp < timing_points_count; tp++) {

                                        // Get "time"
                                        out->timing_points[tp].time = atoi(i);
                                        while (*i != ',') i++; i++; // Skip to next value

                                        // Get "beatLength"
                                        out->timing_points[tp].beat_length = atof(i);
                                        while (*i != ',') i++; i++; // Skip to next value

                                        // Get "meter"
                                        out->timing_points[tp].meter = atoi(i);
                                        while (*i != ',') i++; i++; // Skip to next value

                                        // Skip "sampleSet", "sampleIndex", & "volume"
                                        while (*i != ',') i++; i++;
                                        while (*i != ',') i++; i++;
                                        while (*i != ',') i++; i++;

                                        // Get "uninherited"
                                        out->timing_points[tp].b_uninherited = atoi(i);

                                        // Skip to next line
                                        while (*i != '\n') i++;
                                        i++;

                                }

                                out->timing_points_count = timing_points_count;

                                current_section = SECTION_NONE; // Finished with this section

                        break;



                        case SECTION_HIT_OBJECTS: // Assumed that [HitObjects] is the last section in the file

                                size_t hit_objects_count = 0;
                                for ( // Loop until EOF
                                        int line_offset = 0;
                                        line_offset < beatmap_file_size-(i-beatmap_file_contents);
                                        line_offset++
                                ) {
                                        while (*(i+line_offset) != '\n') line_offset++;
                                        hit_objects_count++;
                                }

                                out->hit_objects = malloc(sizeof(*(out->hit_objects)) * hit_objects_count);

                                for (int ho = 0; ho < hit_objects_count; ho++) {

                                        size_t content_size;

                                        // Get "x"
                                        out->hit_objects[ho].x = atoi(i);
                                        while (*i != ',') i++; i++; // Skip to next value

                                        // Get "y"
                                        out->hit_objects[ho].y = atoi(i);
                                        while (*i != ',') i++; i++; // Skip to next value

                                        // Get "time"
                                        out->hit_objects[ho].time = atoi(i);
                                        while (*i != ',') i++; i++; // Skip to next value

                                        // Get "type"
                                        out->hit_objects[ho].type = atoi(i);
                                        while (*i != ',') i++; i++; // Skip to next value

                                        // Skip "hitSound"
                                        while (*i != ',') i++; i++;

                                        // Get objectParams if slider or spinner
                                        {
                                                // Slider
                                                if (out->hit_objects[ho].type & 0b00000010) {

                                                        // Get "curveType"
                                                        out->hit_objects[ho].object_params.curve_type = *i;
                                                        i += 2; // Skip to next value

                                                        // Get "curvePoints"
                                                        {
                                                                size_t curve_points_count = 0;
                                                                size_t content_size = 0;
                                                                for (
                                                                        ;
                                                                        *(i+content_size) != ',';
                                                                        content_size++
                                                                ) if (*(i+content_size) == ':') curve_points_count++;

                                                                out->hit_objects[ho].object_params.curve_points = malloc(sizeof(*(out->hit_objects[ho].object_params.curve_points)) * curve_points_count);

                                                                // For freeing in FastOsuParser__Free()
                                                                out->_total_curve_points_count++; // *refers to allocated memory blocks... TODO: Better name
                                                                out->_total_curve_points = realloc(out->_total_curve_points, sizeof(void*) * out->_total_curve_points_count);
                                                                out->_total_curve_points[out->_total_curve_points_count-1] = out->hit_objects[ho].object_params.curve_points;

                                                                for (int cp = 0; cp < curve_points_count; cp++) {

                                                                        // Get curvePoint x
                                                                        out->hit_objects[ho].object_params.curve_points[cp].x = atoi(i);

                                                                        while (isdigit(*i)) i++; i++; // Skip to next y

                                                                        // Get curvePoint y
                                                                        out->hit_objects[ho].object_params.curve_points[cp].y = atoi(i);

                                                                        while (isdigit(*i)) i++; i++; // Skip to next x
                                                                }

                                                                out->hit_objects[ho].object_params.curve_points_count = curve_points_count;
                                                        }

                                                        // Get "slides"
                                                        out->hit_objects[ho].object_params.slides = atoi(i);
                                                        while (*i != ',') i++; i++; // Skip to next value

                                                        // Get "length"
                                                        out->hit_objects[ho].object_params.length = atof(i);

                                                        // Implicit "edgeSets" skip

                                                }

                                                // Spinner
                                                else if (out->hit_objects[ho].type & 0b00001000) {

                                                        // Get "endTime"
                                                        out->hit_objects[ho].object_params.end_time = atoi(i);

                                                }
                                        }

                                        // Skip to next line
                                        while (*i != '\n') i++;
                                        i++;

                                }

                                out->hit_objects_count = hit_objects_count;

                                goto _FastOsuParser__Parse_END;

                        break;

                }



                while (*i != '\n') i++; // Skip to beginning of next line

        }



_FastOsuParser__Parse_END:
        free(beatmap_file_contents);

        return FastOsuParser__SUCCESS;

}

void FastOsuParser__Free(FastOsuParser__Beatmap* beatmap) {

        free(beatmap->timing_points);
        free(beatmap->hit_objects);

        for (int i = 0; i < beatmap->_total_curve_points_count; i++) free(beatmap->_total_curve_points[i]);
        free(beatmap->_total_curve_points);

}



#endif // _FAST_OSU_PARSER_H
