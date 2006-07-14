/*
 *  Copyright (C) 2002 Nigel Horne <njh@bandsman.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 *
 * TODO: Allow individual items to be updated or removed
 *
 * It is up to the caller to create a mutex for the table if needed
 */

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#ifndef	CL_DEBUG
#define	NDEBUG	/* map CLAMAV debug onto standard */
#endif

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include "table.h"
#include "others.h"

struct table *
tableCreate(void)
{
	return (struct table *)cli_calloc(1, sizeof(struct table));
}

void
tableDestroy(table_t *table)
{
	tableEntry *tableItem;

	assert(table != NULL);

	tableItem = table->tableHead;

	while(tableItem) {
		tableEntry *tableNext = tableItem->next;

		assert(tableItem->key != NULL);

		if(tableItem->key)
			free(tableItem->key);
		free(tableItem);

		tableItem = tableNext;
	}

	free(table);
}

/*
 * Returns the value, or -1 for failure
 */
int
tableInsert(table_t *table, const char *key, int value)
{
	const int v = tableFind(table, key);

	if(v > 0)	/* duplicate key */
		return (v == value) ? value : -1;	/* allow real dups */

	assert(value != -1);	/* that would confuse us */

	/*
	 * Re-use deleted items
	 */
	if(table->flags&TABLE_HAS_DELETED_ENTRIES) {
		tableEntry *tableItem;

		for(tableItem = table->tableHead; tableItem; tableItem = tableItem->next)
			if(tableItem->key == NULL) {
				/* This item has been deleted */
				tableItem->key = strdup(key);
				tableItem->value = value;
				return value;
			}

		table->flags &= ~TABLE_HAS_DELETED_ENTRIES;
	}

	if(table->tableHead == NULL)
		table->tableLast = table->tableHead = (tableEntry *)cli_malloc(sizeof(tableEntry));
	else
		table->tableLast = table->tableLast->next =
			(tableEntry *)cli_malloc(sizeof(tableEntry));

	if(table->tableLast == NULL)
		return -1;

	table->tableLast->next = NULL;
	table->tableLast->key = strdup(key);
	table->tableLast->value = value;

	return value;
}

/*
 * Returns the value - -1 for not found. This means the value of a valid key
 *	can't be -1 :-(
 */
int
tableFind(const table_t *table, const char *key)
{
	const tableEntry *tableItem;
#ifdef	CL_DEBUG
	int cost;
#endif

	assert(table != NULL);

	if(key == NULL)
		return -1;	/* not treated as a fatal error */

	if(table->tableHead == NULL)
		return -1;	/* not populated yet */

#ifdef	CL_DEBUG
	cost = 0;
#endif

	for(tableItem = table->tableHead; tableItem; tableItem = tableItem->next) {
#ifdef	CL_DEBUG
		cost++;
#endif
		if(strcasecmp(tableItem->key, key) == 0) {
#ifdef	CL_DEBUG
			cli_dbgmsg("tableFind: Cost of '%s' = %d\n", key, cost);
#endif
			return tableItem->value;
		}
	}

	return -1;	/* not found */
}

/*
 * Change a value in the table. If the key isn't in the table insert it
 * Returns -1 for error, otherwise the new value
 */
int
tableUpdate(table_t *table, const char *key, int new_value)
{
	tableEntry *tableItem;

	assert(table != NULL);

	if(key == NULL)
		return -1;	/* not treated as a fatal error */

	if(table->tableHead == NULL)
		/* not populated yet */
		return tableInsert(table, key, new_value);

	for(tableItem = table->tableHead; tableItem; tableItem = tableItem->next)
		if(strcasecmp(tableItem->key, key) == 0) {
			tableItem->value = new_value;
			return new_value;
		}

	/* not found */
	return tableInsert(table, key, new_value);
}

/*
 * Remove an item from the table
 */
void
tableRemove(table_t *table, const char *key)
{
	tableEntry *tableItem;

	assert(table != NULL);

	if(key == NULL)
		return;	/* not treated as a fatal error */

	if(table->tableHead == NULL)
		/* not populated yet */
		return;

	for(tableItem = table->tableHead; tableItem; tableItem = tableItem->next)
		if(strcasecmp(tableItem->key, key) == 0) {
			free(tableItem->key);
			tableItem->key = NULL;
			table->flags |= TABLE_HAS_DELETED_ENTRIES;
			/* don't break, duplicate keys are allowed */
		}
}

void
tableIterate(table_t *table, void(*callback)(char *key, int value))
{
	tableEntry *tableItem;

	if(table == NULL)
		return;

	if(table->tableHead == NULL)
		/* not populated yet */
		return;

	for(tableItem = table->tableHead; tableItem; tableItem = tableItem->next)
		if(tableItem->key)	/* check leaf is not deleted */
			(*callback)(tableItem->key, tableItem->value);

}
