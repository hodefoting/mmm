/* mmm - MicroRaptor Gui
 * Copyright (c) 2014 Øyvind Kolås <pippin@hodefoting.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MMM_LIST__
#define  __MMM_LIST__

#include <stdlib.h>

/* The whole mmm_list implementation is in the header and will be inlined
 * wherever it is used.
 */

typedef struct _MmmList MmmList;
  struct _MmmList {void *data;MmmList *next;
  void (*freefunc)(void *data, void *freefunc_data);
  void *freefunc_data;
}
;

static inline void mmm_list_prepend_full (MmmList **list, void *data,
    void (*freefunc)(void *data, void *freefunc_data),
    void *freefunc_data)
{
  MmmList *new_=calloc (sizeof (MmmList), 1);
  new_->next=*list;
  new_->data=data;
  new_->freefunc=freefunc;
  new_->freefunc_data = freefunc_data;
  *list = new_;
}

static inline int mmm_list_length (MmmList *list)
{
  int length = 0;
  MmmList *l;
  for (l = list; l; l = l->next, length++);
  return length;
}

static inline void mmm_list_prepend (MmmList **list, void *data)
{
  MmmList *new_=calloc (sizeof (MmmList), 1);
  new_->next= *list;
  new_->data=data;
  *list = new_;
}


static inline void mmm_list_append_full (MmmList **list, void *data,
    void (*freefunc)(void *data, void *freefunc_data),
    void *freefunc_data)
{
  MmmList *new_= calloc (sizeof (MmmList), 1);
  new_->data=data;
  new_->freefunc = freefunc;
  new_->freefunc_data = freefunc_data;
  if (*list)
    {
      MmmList *last;
      for (last = *list; last->next; last=last->next);
      last->next = new_;
      return;
    }
  *list = new_;
  return;
}

static inline void mmm_list_append (MmmList **list, void *data)
{
  mmm_list_append_full (list, data, NULL, NULL);
}

static inline void mmm_list_remove (MmmList **list, void *data)
{
  MmmList *iter, *prev = NULL;
  if ((*list)->data == data)
    {
      if ((*list)->freefunc)
        (*list)->freefunc ((*list)->data, (*list)->freefunc_data);
      prev = (void*)(*list)->next;
      free (*list);
      *list = prev;
      return;
    }
  for (iter = *list; iter; iter = iter->next)
    if (iter->data == data)
      {
        if (iter->freefunc)
          iter->freefunc (iter->data, iter->freefunc_data);
        prev->next = iter->next;
        free (iter);
        break;
      }
    else
      prev = iter;
}

static inline void mmm_list_free (MmmList **list)
{
  while (*list)
    mmm_list_remove (list, (*list)->data);
}

static inline MmmList *mmm_list_nth (MmmList *list, int no)
{
  while(no-- && list)
    list = list->next;
  return list;
}

static inline MmmList *mmm_list_find (MmmList *list, void *data)
{
  for (;list;list=list->next)
    if (list->data == data)
      break;
  return list;
}

/* a bubble-sort for now, simplest thing that could be coded up
 * right to make the code continue working
 */
static inline void mmm_list_sort (MmmList **list, 
    int(*compare)(const void *a, const void *b, void *userdata),
    void *userdata)
{
  /* replace this with an insertion sort */
  MmmList *temp = *list;
  MmmList *t;
  MmmList *prev;
again:
  prev = NULL;

  for (t = temp; t; t = t->next)
    {
      if (t->next)
        {
          if (compare (t->data, t->next->data, userdata) > 0)
            {
              /* swap */
              if (prev)
                {
                  MmmList *tnn = t->next->next;
                  prev->next = t->next;
                  prev->next->next = t;
                  t->next = tnn;
                }
              else
                {
                  MmmList *tnn = t->next->next;
                  temp = t->next;
                  temp->next = t;
                  t->next = tnn;
                }
              goto again;
            }
        }
      prev = t;
    }
  *list = temp;
}

static inline void
mmm_list_insert_sorted (MmmList **list, void *data,
                       int(*compare)(const void *a, const void *b, void *userdata),
                       void *userdata)
{
  mmm_list_prepend (list, data);
  mmm_list_sort (list, compare, userdata);
}
#endif
