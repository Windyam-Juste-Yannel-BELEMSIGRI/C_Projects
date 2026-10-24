#include <unistd.h>
#include <fcntl.h>

#define MAX_PIECES 26
#define BUF_SIZE 600
#define BOARD_MAX 32

typedef struct s_piece
{
	int		x[4];
	int		y[4];
	int		pos;
	char	letter;
}			t_piece;

static char	g_board[BOARD_MAX][BOARD_MAX];

static int	error(void)
{
	write(1, "error\n", 6);
	return (1);
}

/* ---------- PARSING ---------- */

static int	parse_piece(const char *s, t_piece *p)
{
	int	i;
	int	n;
	int	links;
	int	min[2];

	i = -1;
	n = 0;
	links = 0;
	while (++i < 20)
	{
		if (i % 5 == 4 && s[i] != '\n')
			return (0);
		if (i % 5 == 4)
			continue ;
		if (s[i] != '.' && s[i] != '#')
			return (0);
		if (s[i] == '#' && n++ < 4)
		{
			p->x[n - 1] = i % 5;
			p->y[n - 1] = i / 5;
			links += (i % 5 > 0 && s[i - 1] == '#') + (i >= 5 && s[i - 5] == '#');
		}
	}
	if (n != 4 || links < 3)
		return (0);
	min[0] = 4;
	min[1] = 4;
	i = -1;
	while (++i < 4)
	{
		if (p->x[i] < min[0])
			min[0] = p->x[i];
		if (p->y[i] < min[1])
			min[1] = p->y[i];
	}
	i = -1;
	while (++i < 4)
	{
		p->x[i] -= min[0];
		p->y[i] -= min[1];
	}
	return (1);
}

static int	read_file(const char *path, char *buf)
{
	int	fd;
	int	ret;
	int	total;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (-1);
	total = 0;
	while ((ret = read(fd, buf + total, BUF_SIZE - total)) > 0)
		total += ret;
	close(fd);
	if (ret < 0 || total >= BUF_SIZE)
		return (-1);
	return (total);
}

static int	parse_all(const char *path, t_piece *p)
{
	char	buf[BUF_SIZE];
	int		total;
	int		n;
	int		k;

	total = read_file(path, buf);
	if (total < 0 || total % 21 != 20)
		return (-1);
	n = (total + 1) / 21;
	if (n > MAX_PIECES)
		return (-1);
	k = -1;
	while (++k < n)
	{
		if (!parse_piece(buf + 21 * k, &p[k]))
			return (-1);
		if (k < n - 1 && buf[21 * k + 20] != '\n')
			return (-1);
		p[k].letter = 'A' + k;
	}
	return (n);
}

/* ---------- RESOLUTION (backtracking) ---------- */

static int	can_place(t_piece *p, int x, int y, int size)
{
	int	k;
	int	cx;
	int	cy;

	k = -1;
	while (++k < 4)
	{
		cx = x + p->x[k];
		cy = y + p->y[k];
		if (cx >= size || cy >= size || g_board[cy][cx] != '.')
			return (0);
	}
	return (1);
}

static void	put(t_piece *p, int pos, int size, char c)
{
	int	k;

	k = -1;
	while (++k < 4)
		g_board[pos / size + p->y[k]][pos % size + p->x[k]] = c;
}

static int	same(t_piece *a, t_piece *b)
{
	int	k;

	k = -1;
	while (++k < 4)
		if (a->x[k] != b->x[k] || a->y[k] != b->y[k])
			return (0);
	return (1);
}

static int	solve(t_piece *p, int n, int i, int size)
{
	int	pos;

	if (i == n)
		return (1);
	pos = 0;
	if (i > 0 && same(&p[i], &p[i - 1]))
		pos = p[i - 1].pos + 1;
	while (pos < size * size)
	{
		if (can_place(&p[i], pos % size, pos / size, size))
		{
			put(&p[i], pos, size, p[i].letter);
			p[i].pos = pos;
			if (solve(p, n, i + 1, size))
				return (1);
			put(&p[i], pos, size, '.');
		}
		pos++;
	}
	return (0);
}

/* ---------- MAIN ---------- */

int			main(int argc, char **argv)
{
	t_piece	p[MAX_PIECES];
	int		n;
	int		size;
	int		i;
	int		j;

	if (argc != 2)
	{
		write(1, "usage: ./fillit source_file\n", 28);
		return (1);
	}
	if ((n = parse_all(argv[1], p)) < 1)
		return (error());
	i = -1;
	while (++i < BOARD_MAX && (j = -1))
		while (++j < BOARD_MAX)
			g_board[i][j] = '.';
	size = 2;
	while (size * size < 4 * n)
		size++;
	while (!solve(p, n, 0, size))
		size++;
	i = -1;
	while (++i < size)
	{
		write(1, g_board[i], size);
		write(1, "\n", 1);
	}
	return (0);
}
