#include "option.h"
#include <stdio.h>

void		option_allocation_failure(void)
{
	perror("Unable to allocate option table");
	exit(EXIT_FAILURE);
}

option_t	*insert_option(option_list_t *list, const char *name)
{
	if (list->size == list->max_size)
	{
		list->max_size += (!list->max_size) ? 16 : list->max_size;

		list->options = realloc(list->options, list->max_size * sizeof(option_t));

		if (!list->options)
			option_allocation_failure();
	}

	// Binary search to find the index of our new option.

	size_t	left = 0;
	size_t	right = list->size;
	size_t	i;

	while (left < right)
	{
		i = (left + right) / 2;
		if (strcmp(name, list->options[i].name) < 0)
			right = i;
		else
			left = i + 1;
	}
	memmove(&list->options[left + 1], &list->options[left],
		sizeof(option_t) * (list->size - left));
	memset(&list->options[left], 0, sizeof(option_t));

	list->options[left].name = strdup(name);

	if (!list->options[left].name)
		option_allocation_failure();

	list->size++;

	return (&list->options[left]);
}

void	add_option_spin_int(option_list_t *list, const char *name, long *data,
		long def, long min, long max, void (*callback)(void *))
{
	option_t	*cur = insert_option(list, name);

	cur->type = OptionSpinInt;
	cur->data = data;
	cur->callback = callback;
	cur->def = malloc(sizeof(long));
	cur->min = malloc(sizeof(long));
	cur->max = malloc(sizeof(long));
	if (!cur->def || !cur->min || !cur->max)
		option_allocation_failure();

	*(long *)cur->def = def;
	*(long *)cur->min = min;
	*(long *)cur->max = max;
}

void	add_option_spin_flt(option_list_t *list, const char *name, double *data,
		double def, double min, double max, void (*callback)(void *))
{
	option_t	*cur = insert_option(list, name);

	cur->type = OptionSpinFlt;
	cur->data = data;
	cur->callback = callback;
	cur->def = malloc(sizeof(double));
	cur->min = malloc(sizeof(double));
	cur->max = malloc(sizeof(double));
	if (!cur->def || !cur->min || !cur->max)
		option_allocation_failure();

	*(double *)cur->def = def;
	*(double *)cur->min = min;
	*(double *)cur->max = max;
}

void	add_option_check(option_list_t *list, const char *name, bool *data,
		bool def, void (*callback)(void *))
{
	option_t	*cur = insert_option(list, name);

	cur->type = OptionCheck;
	cur->data = data;
	cur->callback = callback;
	cur->def = malloc(sizeof(bool));
	if (!cur->def)
		option_allocation_failure();
	*(bool *)cur->def = def;
}

void	add_option_combo(option_list_t *list, const char *name, char **data,
		const char *def, const char *const *combo_list, void (*callback)(void *))
{
	option_t	*cur = insert_option(list, name);

	cur->type = OptionCombo;
	cur->data = data;
	cur->callback = callback;
	cur->def = strdup(def);

	if (!cur->def)
		option_allocation_failure();

	size_t	length;
	for (length = 0; combo_list[length]; ++length);

	cur->combo_list = malloc(sizeof(char *) * (length + 1));
	if (!cur->combo_list)
		option_allocation_failure();

	for (size_t i = 0; i < length; ++i)
	{
		cur->combo_list[i] = strdup(combo_list[i]);
		if (!cur->combo_list[i])
			option_allocation_failure();
	}
	cur->combo_list[length] = NULL;
}

void	add_option_button(option_list_t *list, const char *name,
		void (*callback)(void *))
{
	option_t	*cur = insert_option(list, name);

	cur->type = OptionButton;
	cur->callback = callback;
}

void	add_option_string(option_list_t *list, const char *name, char **data,
		const char *def, void (*callback)(void *))
{
	option_t	*cur = insert_option(list, name);

	cur->type = OptionString;
	cur->data = data;
	cur->callback = callback;
	cur->def = strdup(def);

	if (!cur->def)
		option_allocation_failure();
}