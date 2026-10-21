/*  Fondement Michtam
 *  Copyright (C) 2024 Xavier Lacroix
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _Tools_Class_h_
#define _Tools_Class_h_

/*_______________________________________________________________________
 |
 | s_(t) size of a base type or structure
 | sv_(v) size of the variable pointed by v
 | snv_(v,n) size of the array of n elements pointed by v
 | o_(t,d) offset of field d in the structure t
 | c_(module,category,constant) constant implementation as offsets in a 
 |     structure declared as "struct c_##module". Each category is a
 | substructure of struct c_##module.
 | v_(t,d,p) given the address p of the d field, return the address of 
 |     the surrounding structure.
 |_______________________________________________________________________
*/

#define s_(t) (((void *)(((t *)0)+1))-((void *)0))
#define sv_(v) (((void *)((v)+1))-((void *)(v)))
#define snv_(v,n) (((void *)((v)+(n)))-((void *)(v)))
#define o_(t,d) (((char *)(&((t *)0)->d))-((char *)0))
#define c_(t,d,p) (((char *)(&(((struct c_##t *)0)->d.p)))-((char *)(&((struct c_##t *)0)->d)))
#define v_(t,d,p) ((t *)(((char *)(p))-o_(t,d)))

/*________________________________________________________________________
 |
 | Basic data structure components:
 |
 | Use these components to build your overall data structures.
 | Embed them in your structures, operate them using member pointers,
 | and use v_() to retrieve the embedding variable.
 | structure.
 | When embedding them in structure, it is customary to hint at their 
 | connection. Use the of_() macro to do so.
 |
 | example:
 | typedef struct TlsDList TlsDList;
 | struct TlsDList { TlsDList *p,*n; }; 
 |
 | typedef struct {
 |     TlsDList of_(PointList.List) List;
 |     struct Point{int x[3];} x;
 | } PointList;
 | typedef struct {
 |     TlsDList of_(PolygonList.List) List;
 |     TlsDList of_(PointList.List) Polygon;
 | } PolygonList;
 |________________________________________________________________________
*/

#define of_(spec_type) /**/

typedef struct TlsList of_(x) TlsList of_(x);
struct TlsList of_(x) { TlsList of_(x) *n; };
extern TlsList TlsListEnd;

typedef struct TlsDList TlsDList;
struct TlsDList { TlsDList *p,*n; };

/*_______________________________________________________________________
 |
 | Class(t) is a typedef structure 't' with a pointer to a static 
 | structure struct t that define methods and shared members.
 | 
 | The first parameters of a method is the pointer on the object "this",
 |     ThisToThat declares a pointer on the specialised object "that",
 | which should contain the specialised fields.
 | 
 | x_(o,m,...) invokes the objects o methods m. 
 |    !!! Be careful because the o will be evaluated twice. !!!
 |_______________________________________________________________________
*/

#define TlsClassDef(t) typedef struct {struct t *Static;} t; struct t
#define TlsGetThat(t,d) t *that=v_(t,d,this)
#define x_(v,m,...) ((v)->Static->m)(v __VA_OPT__(,) __VA_ARGS__)

#endif


