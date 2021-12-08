#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
//#include <unistd.h>
//#include <poll.h>
#include <ncurses.h>
#include "digits.h"

const wchar_t* DRAWING_GLYPH = L"█";
const int REFRESH_TIME = 1000; // milliseconds
int middle_x = 0;
int middle_y = 1;

void render_digits(const char* num_str, const size_t num_str_len)
{
	unsigned int coercd_digit = 0;
	int current_x = middle_x, current_y = (middle_y - 3);

	// Glyphs are drawn from top to bottom, instead of left to right.
	// That means that it renders one "scanline" of each glyph in the string at once
	// Not the most efficient way, but this is my solution on drawing multiple glyphs on the same line
	for (size_t rep = 0; rep < GLYPH_LINE_HEIGHT; ++rep) { // scanlines
		for (size_t i = 0; i < num_str_len; ++i) { // iterate over each character of the string
			coercd_digit = (unsigned int)(num_str[i] - '0'); // convert character to integer
			if (coercd_digit > 9) {
				if (num_str[i] == ':')
					coercd_digit = 10;
				else if (num_str[i] == '/')
					coercd_digit = 11;
				else if (num_str[i] == ' ')
					coercd_digit = 12;
				else // ignore any non-numerical char
					continue;
			}
			// move cursor to the middles and make sure the glyph is still centered relative to the center x coord
			move(current_y, current_x - ((num_str_len * 7) >> 1));
			for (ssize_t j = GLYPH_LINE_WIDTH - 1; j >= 0; --j) { // draw the current scanline of the glyph
				printw("%ls", default_font[coercd_digit][rep] & (1 << j) ? DRAWING_GLYPH : L" ");
				++current_x;
			}
			++current_x; // space between each glyph
		}
		current_x = middle_x;
		++current_y;
	}
}

// ncurses border function only handles signed ASCII. There's border_set() but setting up the complex chars is too much of a hassle
void extended_border(const wchar_t* ls, const wchar_t* rs, const wchar_t* ts, const wchar_t* bs,
					const wchar_t* tl, const wchar_t* tr, const wchar_t* bl, const wchar_t* br)
{
	int max_y = 0;
	int max_x = 0;
	getmaxyx(stdscr, max_y, max_x);
	--max_y, --max_x;
	for (int i = 1; i <= max_x; ++i) { // top and bottom
		mvprintw(0, i, "%ls", ts);
		mvprintw(max_y, i, "%ls", bs);
	}
	for (int j = 1; j <= max_y; ++j) { // left and right
		mvprintw(j, 0, "%ls", ls);
		mvprintw(j, max_x, "%ls", rs);
	}
	// finally the corners
	mvprintw(0, 0, "%ls", tl);
	mvprintw(0, max_x, "%ls", tr);
	mvprintw(max_y, 0, "%ls", bl);
	mvprintw(max_y, max_x, "%ls", br);
}

void terminal_get_centers(int* y, int* x)
{
	getmaxyx(stdscr, *y, *x); // get console maximum size
	// get halfs
	*y >>= 1;
	*x >>= 1;
}

void terminal_resize_handler()
{
	endwin();
	terminal_get_centers(&middle_y, &middle_x);
	clear();
	extended_border(L"║", L"║", L"═", L"═", L"╔", L"╗", L"╚", L"╝");
	refresh();
}

void mainloop()
{
	char time_buffer[64] = {0};
	char date_buffer[64] = {0};
	// struct pollfd p_fd[1] = {{
	// 	.fd = STDIN_FILENO,
	// 	.events = POLLIN,
	// }};

	size_t date_fmt_len = 0;
	int last_day = 0;
	int current_color = 2;
	bool disp_date = true;

	timeout(REFRESH_TIME);
	for (;;) {
		time_t current_time = time(NULL);
		struct tm* current_time_info = localtime(&current_time);
		size_t time_fmt_len = strftime(time_buffer, sizeof(time_buffer), "%I:%M:%S", current_time_info); // %_2I

		if (current_time_info->tm_yday != last_day) // update date only when necessary
			date_fmt_len = strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", current_time_info);
		last_day = current_time_info->tm_yday;
		// draw everything
		render_digits(time_buffer, time_fmt_len);
		if (disp_date) {
			attron(A_BOLD);
			mvprintw(middle_y + 3, (middle_x - date_fmt_len) + 4, date_buffer);
			attroff(A_BOLD);
		}
		refresh();

		// if (poll(p_fd, 1, REFRESH_TIME) < 0 && errno != EINTR) {
		// 	endwin();
		// 	perror("poll");
		// 	break;
		// }
		int chr;
		//if (p_fd[0].revents & POLLIN) {
		if ((chr = getch()) != ERR) {
			switch (chr) {
				case 'q': return;
				case 'd':
					disp_date = !disp_date;
					clear();
					extended_border(L"║", L"║", L"═", L"═", L"╔", L"╗", L"╚", L"╝");
					break;
				case 'c':
					attron(COLOR_PAIR(current_color++));
					if (!current_color || current_color > 4)
						current_color = 1;
					extended_border(L"║", L"║", L"═", L"═", L"╔", L"╗", L"╚", L"╝");
					break;
				case 'r': // redraw
					terminal_resize_handler();
					break;
			}
		}
	}
}

int main()
{
	if (!setlocale(LC_ALL, "")) // TODO: fallback to ASCII borders (+-)
		return 1;

	signal(SIGWINCH, terminal_resize_handler); // handle terminal resizing
	// ncurses initialization
	initscr();
	if (has_colors()) {
		start_color();
		use_default_colors();

		init_pair(1, COLOR_RED, -1);
		init_pair(2, COLOR_GREEN, -1);
		init_pair(3, COLOR_YELLOW, -1);
		init_pair(4, COLOR_BLUE, -1);
		attron(COLOR_PAIR(1));
	}

	cbreak();
	noecho();
	curs_set(0);
	terminal_get_centers(&middle_y, &middle_x);
	extended_border(L"║", L"║", L"═", L"═", L"╔", L"╗", L"╚", L"╝");

	mainloop();

	endwin();
	return 0;
}
