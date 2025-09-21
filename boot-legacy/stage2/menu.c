/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2025 Omar Elghoul
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <boot/menu.h>
#include <boot/output.h>
#include <boot/input.h>
#include <stdio.h>
#include <string.h>

static void print_centered(const char *str) {
    display.x = (CONSOLE_WIDTH / 2) - (strlen(str) / 2);
    printf("%s", str);
}

static void render_items(MenuState *state) {
    display.y = 2;
    display.x = 2;

    for(int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        int item = state->top_visible_index + i;

        if(item >= state->count || !state->items[item]) {
            display.bg = palette[BLACK];
            printf("%-76s", "");
        } else {
            if(item == state->selected) {
                display.bg = palette[GREEN];
                display.fg = palette[BLACK];
            } else {
                display.bg = palette[BLACK];
                display.fg = palette[LIGHT_GRAY];
            }
            printf(" %-75s", state->items[item]);
        }

        display.y++;
        display.x = 2;
    }
}

static void render_menu(MenuState *state, int allow_escape) {
    clear_screen();

    display.bg = palette[BROWN];
    display.fg = palette[WHITE];
    display.y = 0;
    display.x = 0;
    printf("%80c", ' ');    // title bar

    display.y = 0;
    print_centered(state->title);

    render_items(state);

    display.y = 3 + MAX_VISIBLE_ROWS;
    display.bg = palette[BLACK];
    display.fg = palette[WHITE];

    if(!allow_escape) print_centered("<Up/Down> to navigate, <Enter> to select.");
    else print_centered("<Up/Down> to navigate, <Enter> to select, <Esc> to go back.");

    display.y += 2;
    print_centered("Kiwi is free and open-source software released under the MIT License.");
}

int drive_menu(MenuState *state, int allow_escape) {
    render_menu(state, allow_escape);

    Character ch;
    for(;;) {
        if(!input_read(&ch)) continue;

        if(ch.scan_code == SCANCODE_UP) {
            if(state->selected > 0) {
                state->selected--;

                while(state->items[state->selected] == NULL && state->selected > 0) {
                    state->selected--;
                }

                if(state->selected <= state->top_visible_index) {
                    state->top_visible_index = state->selected;
                }
                render_items(state);
            }
        } else if(ch.scan_code == SCANCODE_DOWN) {
            if(state->selected < state->count - 1) {
                state->selected++;

                while(state->items[state->selected] == NULL && state->selected < state->count - 1) {
                    state->selected++;
                }

                if(state->selected >= state->top_visible_index + MAX_VISIBLE_ROWS) {
                    state->top_visible_index = state->selected - MAX_VISIBLE_ROWS + 1;
                }
                render_items(state);
            }
        } else if(ch.scan_code == SCANCODE_ENTER) {
            return state->selected;
        } else if(ch.scan_code == SCANCODE_ESCAPE && allow_escape) {
            return -1;
        }
    }

    return -1;
}

#define DIALOG_MARGIN               24
#define DIALOG_BORDER_COLOR         GREEN
#define DIALOG_BACKGROUND_COLOR     BLACK
#define DIALOG_TEXT_COLOR           LIGHT_GRAY
#define DIALOG_TITLE_COLOR          LIGHT_GREEN
#define DIALOG_BORDER_THICKNESS     3

void dialog(const char *title, const char *message, int width, int height) {
    dim_screen();
    display.x = (CONSOLE_WIDTH / 2) - (width / 2);
    display.y = (CONSOLE_HEIGHT / 2) - (height / 2);

    u16 x = display.x * FONT_WIDTH
        + ((display.current_mode->width/2
        - (CONSOLE_WIDTH*FONT_WIDTH)/2));
    u16 y = display.y * FONT_HEIGHT
        + ((display.current_mode->height/2
        - (CONSOLE_HEIGHT*FONT_HEIGHT)/2));

    fill_rect(x - DIALOG_MARGIN,
              y - DIALOG_MARGIN,
              width * FONT_WIDTH + (DIALOG_MARGIN * 2),
              DIALOG_BORDER_THICKNESS,
              palette[DIALOG_BORDER_COLOR]);
    
    fill_rect(x - DIALOG_MARGIN,
              y - DIALOG_MARGIN,
              DIALOG_BORDER_THICKNESS,
              height * FONT_HEIGHT + (DIALOG_MARGIN * 2),
              palette[DIALOG_BORDER_COLOR]);
    
    fill_rect(x + width * FONT_WIDTH + DIALOG_MARGIN - DIALOG_BORDER_THICKNESS,
              y - DIALOG_MARGIN,
              DIALOG_BORDER_THICKNESS,
              height * FONT_HEIGHT + (DIALOG_MARGIN * 2),
              palette[DIALOG_BORDER_COLOR]);

    fill_rect(x - DIALOG_MARGIN,
              y + height * FONT_HEIGHT + DIALOG_MARGIN - DIALOG_BORDER_THICKNESS,
              width * FONT_WIDTH + (DIALOG_MARGIN * 2),
              DIALOG_BORDER_THICKNESS,
              palette[DIALOG_BORDER_COLOR]);

    fill_rect(x - DIALOG_MARGIN + DIALOG_BORDER_THICKNESS,
              y - DIALOG_MARGIN + DIALOG_BORDER_THICKNESS,
              width * FONT_WIDTH + (DIALOG_MARGIN * 2) - (DIALOG_BORDER_THICKNESS * 2),
              height * FONT_HEIGHT + (DIALOG_MARGIN * 2) - (DIALOG_BORDER_THICKNESS * 2),
              palette[DIALOG_BACKGROUND_COLOR]);

    display.bg = palette[DIALOG_BACKGROUND_COLOR];
    display.fg = palette[DIALOG_TITLE_COLOR];

    printf("%s", title);

    display.x = (CONSOLE_WIDTH / 2) - (width / 2);
    display.y += 2;
    display.fg = palette[DIALOG_TEXT_COLOR];

    for(; *message; message++) {
        if(*message == '\n') {
            display.x = (CONSOLE_WIDTH / 2) - (width / 2);
            display.y++;
            continue;
        }
        printf("%c", *message);
    }

    display.x = (CONSOLE_WIDTH / 2) - (width / 2);
    display.y = (CONSOLE_HEIGHT / 2) + (height / 2) - 1;

    display.fg = palette[WHITE];
    print_centered("<Enter> or <Esc> to go back.");

    for(;;) {
        Character ch;
        if(!input_read(&ch)) continue;

        if(ch.scan_code == SCANCODE_ENTER || ch.scan_code == SCANCODE_ESCAPE) {
            return;
        }
    }
        
}