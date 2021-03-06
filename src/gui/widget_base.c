﻿/* ***************************************************************************
 * widget_base.c -- the widget base operation set.
 *
 * Copyright (C) 2012-2017 by Liu Chao <lc-soft@live.cn>
 *
 * This file is part of the LCUI project, and may only be used, modified, and
 * distributed under the terms of the GPLv2.
 *
 * (GPLv2 is abbreviation of GNU General Public License Version 2)
 *
 * By continuing to use, modify, or distribute this file you indicate that you
 * have read the license and understand and accept it fully.
 *
 * The LCUI project is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GPL v2 for more details.
 *
 * You should have received a copy of the GPLv2 along with this file. It is
 * usually in the LICENSE.TXT file, If not, see <http://www.gnu.org/licenses/>.
 * ****************************************************************************/

/* ****************************************************************************
 * widget_base.c -- 部件的基本操作集。
 *
 * 版权所有 (C) 2012-2017 归属于 刘超 <lc-soft@live.cn>
 *
 * 这个文件是LCUI项目的一部分，并且只可以根据GPLv2许可协议来使用、更改和发布。
 *
 * (GPLv2 是 GNU通用公共许可证第二版 的英文缩写)
 *
 * 继续使用、修改或发布本文件，表明您已经阅读并完全理解和接受这个许可协议。
 *
 * LCUI 项目是基于使用目的而加以散布的，但不负任何担保责任，甚至没有适销性或特
 * 定用途的隐含担保，详情请参照GPLv2许可协议。
 *
 * 您应已收到附随于本文件的GPLv2许可协议的副本，它通常在LICENSE.TXT文件中，如果
 * 没有，请查看：<http://www.gnu.org/licenses/>.
 * ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/gui/widget.h>
#include <LCUI/gui/metrics.h>

#define WIDGET_SIZE (sizeof(LCUI_WidgetRec) + sizeof(LinkedListNode) * 2)

static struct LCUIWidgetModule {
	LCUI_Widget root;		/**< 根级部件 */
	Dict *ids;			/**< 各种部件的ID索引 */
	LCUI_Mutex mutex;		/**< 互斥锁 */
	DictType dt_attributes;		/**< 部件属性表的类型模板 */
} LCUIWidget;

#define StrList_Destroy freestrs

LCUI_Widget LCUIWidget_GetRoot( void )
{
	return LCUIWidget.root;
}

/** 刷新部件的状态 */
static void Widget_UpdateStatus( LCUI_Widget widget )
{
	LCUI_Widget child;
	if( !widget->parent ) {
		return;
	}
	if( widget->index == widget->parent->children.length - 1 ) {
		Widget_AddStatus( widget, "last-child" );
		child = Widget_GetPrev( widget );
		if( child ) {
			Widget_RemoveStatus( child, "last-child" );
		}
	}
	if( widget->index == 0 ) {
		Widget_AddStatus( widget, "first-child" );
		child = Widget_GetNext( widget );
		if( child ) {
			Widget_RemoveStatus( child, "first-child" );
		}
	}
}

int Widget_Unlink( LCUI_Widget widget )
{
	LCUI_Widget child;
	LinkedListNode *node, *snode;
	if( !widget->parent ) {
		return -1;
	}
	node = Widget_GetNode( widget );
	snode = Widget_GetShowNode( widget );
	if( widget->index == widget->parent->children.length - 1 ) {
		Widget_RemoveStatus( widget, "last-child" );
		child = Widget_GetPrev( widget );
		if( child ) {
			Widget_AddStatus( child, "last-child" );
		}
	}
	if( widget->index == 0 ) {
		Widget_RemoveStatus( widget, "first-child" );
		child = Widget_GetNext( widget );
		if( child ) {
			Widget_AddStatus( child, "first-child" );
		}
	}
	/** 修改它后面的部件的 index 值 */
	node = node->next;
	while( node ) {
		child = node->data;
		child->index -= 1;
		node = node->next;
	}
	node = Widget_GetNode( widget );
	LinkedList_Unlink( &widget->parent->children, node );
	LinkedList_Unlink( &widget->parent->children_show, snode );
	Widget_PostSurfaceEvent( widget, WET_REMOVE );
	widget->parent = NULL;
	return 0;
}

int Widget_Append( LCUI_Widget parent, LCUI_Widget widget )
{
	LCUI_Widget child;
	LinkedListNode *node, *snode;
	if( !parent || !widget ) {
		return -1;
	}
	if( parent == widget ) {
		return -2;
	}
	Widget_Unlink( widget );
	widget->parent = parent;
	widget->state = WSTATE_CREATED;
	widget->index = parent->children.length;
	node = Widget_GetNode( widget );
	snode = Widget_GetShowNode( widget );
	LinkedList_AppendNode( &parent->children, node );
	LinkedList_AppendNode( &parent->children_show, snode );
	/** 修改它后面的部件的 index 值 */
	node = node->next;
	while( node ) {
		child = node->data;
		child->index += 1;
		node = node->next;
	}
	Widget_PostSurfaceEvent( widget, WET_ADD );
	Widget_AddTaskForChildren( widget, WTT_REFRESH_STYLE );
	Widget_UpdateTaskStatus( widget );
	Widget_UpdateStatus( widget );
	Widget_UpdateLayout( parent );
	return 0;
}

int Widget_Prepend( LCUI_Widget parent, LCUI_Widget widget )
{
	LCUI_Widget child;
	LinkedListNode *node, *snode;
	if( !parent || !widget ) {
		return -1;
	}
	if( parent == widget ) {
		return -2;
	}
	child = widget->parent;
	Widget_Unlink( widget );
	widget->index = 0;
	widget->parent = parent;
	widget->state = WSTATE_CREATED;
	node = Widget_GetNode( widget );
	snode = Widget_GetShowNode( widget );
	LinkedList_InsertNode( &parent->children, 0, node );
	LinkedList_InsertNode( &parent->children_show, 0, snode );
	/** 修改它后面的部件的 index 值 */
	node = node->next;
	while( node ) {
		child = node->data;
		child->index += 1;
		node = node->next;
	}
	Widget_PostSurfaceEvent( widget, WET_ADD );
	Widget_AddTaskForChildren( widget, WTT_REFRESH_STYLE );
	Widget_UpdateTaskStatus( widget );
	Widget_UpdateStatus( widget );
	Widget_UpdateLayout( parent );
	return 0;
}

int Widget_Unwrap( LCUI_Widget widget )
{
	int i;
	LCUI_Widget child;
	LinkedList *list, *list_show;
	LinkedListNode *target, *node, *prev, *snode;

	if( !widget->parent ) {
		return -1;
	}
	list = &widget->parent->children;
	list_show = &widget->parent->children_show;
	if( widget->children.length > 0 ) {
		node = LinkedList_GetNode( &widget->children, 0 );
		Widget_RemoveStatus( node->data, "first-child" );
		node = LinkedList_GetNode( &widget->children, -1 );
		Widget_RemoveStatus( node->data, "last-child" );
	}
	node = Widget_GetNode( widget );
	i = widget->children.length;
	target = node->prev;
	node = widget->children.tail.prev;
	while( i-- > 0 ) {
		prev = node->prev;
		child = node->data;
		snode = Widget_GetShowNode( child );
		LinkedList_Unlink( &widget->children, node );
		LinkedList_Unlink( &widget->children_show, snode );
		child->parent = widget->parent;
		LinkedList_Link( list, target, node );
		LinkedList_AppendNode( list_show, snode );
		Widget_AddTaskForChildren( child, WTT_REFRESH_STYLE );
		Widget_UpdateTaskStatus( child );
		node = prev;
	}
	if( widget->index == 0 ) {
		Widget_AddStatus( target->next->data, "first-child" );
	}
	if( widget->index == list->length - 1 ) {
		node = LinkedList_GetNode( list, -1 );
		Widget_AddStatus( node->data, "last-child" );
	}
	Widget_Destroy( widget );
	return 0;
}

/** 构造函数 */
static void Widget_Init( LCUI_Widget widget )
{
	ZEROSET( widget, LCUI_Widget );
	widget->state = WSTATE_CREATED;
	widget->trigger = EventTrigger();
	widget->style = StyleSheet();
	widget->custom_style = StyleSheet();
	widget->inherited_style = StyleSheet();
	widget->computed_style.opacity = 1.0;
	widget->computed_style.visible = TRUE;
	widget->computed_style.focusable = FALSE;
	widget->computed_style.display = SV_BLOCK;
	widget->computed_style.position = SV_STATIC;
	widget->computed_style.pointer_events = SV_AUTO;
	widget->computed_style.box_sizing = SV_CONTENT_BOX;
	widget->computed_style.margin.top.type = SVT_PX;
	widget->computed_style.margin.right.type = SVT_PX;
	widget->computed_style.margin.bottom.type = SVT_PX;
	widget->computed_style.margin.left.type = SVT_PX;
	widget->computed_style.padding.top.type = SVT_PX;
	widget->computed_style.padding.right.type = SVT_PX;
	widget->computed_style.padding.bottom.type = SVT_PX;
	widget->computed_style.padding.left.type = SVT_PX;
	Background_Init( &widget->computed_style.background );
	BoxShadow_Init( &widget->computed_style.shadow );
	Border_Init( &widget->computed_style.border );
	LinkedList_Init( &widget->children );
	LinkedList_Init( &widget->children_show );
	LinkedList_Init( &widget->dirty_rects );
	Graph_Init( &widget->graph );
}

LCUI_Widget LCUIWidget_New( const char *type )
{
	LinkedListNode *node;
	LCUI_Widget widget = malloc( WIDGET_SIZE );

	Widget_Init( widget );
	node = Widget_GetNode( widget );
	node->data = widget;
	node->next = node->prev = NULL;
	node = Widget_GetShowNode( widget );
	node->data = widget;
	node->next = node->prev = NULL;
	if( type ) {
		widget->proto = LCUIWidget_GetPrototype( type );
		if( widget->proto ) {
			widget->type = widget->proto->name;
			widget->proto->init( widget );
		} else {
			widget->type = strdup( type );
		}
	}
	Widget_AddTask( widget, WTT_REFRESH_STYLE );
	return widget;
}

static void Widget_OnDestroy( void *arg )
{
	Widget_ExecDestroy( arg );
}

void Widget_ExecDestroy( LCUI_Widget widget )
{
	LCUI_WidgetEventRec e = { WET_DESTROY, 0 };
	Widget_TriggerEvent( widget, &e, NULL );
	Widget_ReleaseMouseCapture( widget );
	Widget_ReleaseTouchCapture( widget, -1 );
	Widget_StopEventPropagation( widget );
	LCUIWidget_ClearEventTarget( widget );
	/* 先释放显示列表，后销毁部件列表，因为部件在这两个链表中的节点是和它共用
	 * 一块内存空间的，销毁部件列表会把部件释放掉，所以把这个操作放在后面 */
	LinkedList_ClearData( &widget->children_show, NULL );
	LinkedList_ClearData( &widget->children, Widget_OnDestroy );
	if( widget->proto && widget->proto->destroy ) {
		widget->proto->destroy( widget );
	}
	RectList_Clear( &widget->dirty_rects );
	StyleSheet_Delete( widget->inherited_style );
	StyleSheet_Delete( widget->custom_style );
	StyleSheet_Delete( widget->style );
	if( widget->parent ) {
		Widget_UpdateLayout( widget->parent );
	}
	Widget_SetId( widget, NULL );
	if( widget->type && !widget->proto ) {
		free( widget->type );
		widget->type = NULL;
	}
	widget->proto = NULL;
	if( widget->title ) {
		free( widget->title );
		widget->title = NULL;
	}
	widget->attributes ? Dict_Release( widget->attributes ) : 0;
	widget->classes ? StrList_Destroy( widget->classes ) : 0;
	widget->status ? StrList_Destroy( widget->status ) : 0;
	EventTrigger_Destroy( widget->trigger );
	widget->trigger = NULL;
	free( widget );
}

void Widget_Destroy( LCUI_Widget w )
{
	LCUI_Widget root = w;
	while( root->parent ) {
		root = root->parent;
	}
	if( root != LCUIWidget.root ) {
		LCUI_WidgetEventRec e = { 0 };
		e.type = WET_REMOVE;
		w->state = WSTATE_DELETED;
		Widget_TriggerEvent( w, &e, NULL );
		Widget_ExecDestroy( w );
		return;
	}
	if( w->parent ) {
		LCUI_Widget child;
		LinkedListNode *node;
		node = Widget_GetNode( w );
		node = node->next;
		while( node ) {
			child = node->data;
			child->index -= 1;
			node = node->next;
		}
		if( w->computed_style.position != SV_ABSOLUTE ) {
			Widget_UpdateLayout( w->parent );
		}
		Widget_PushInvalidArea( w, NULL, SV_GRAPH_BOX );
		Widget_AddToTrash( w );
	}
}

void Widget_Empty( LCUI_Widget w )
{
	LCUI_Widget root = w;

	while( root->parent ) {
		root = root->parent;
	}
	if( root == LCUIWidget.root ) {
		LinkedListNode *next, *node;
		node = w->children.head.next;
		while( node ) {
			next = node->next;
			Widget_AddToTrash( node->data );
			node = next;
		}
		Widget_PushInvalidArea( w, NULL, SV_GRAPH_BOX );
		Widget_AddTask( w, WTT_LAYOUT );
	} else {
		LinkedList_ClearData( &w->children_show, NULL );
		LinkedList_ClearData( &w->children, Widget_OnDestroy );
	}
}

LCUI_Widget Widget_At( LCUI_Widget widget, int x, int y )
{
	LCUI_BOOL is_hit;
	LinkedListNode *node;
	LCUI_Widget target = widget, c = NULL;
	if( !widget ) {
		return NULL;
	}
	do {
		is_hit = FALSE;
		for( LinkedList_Each( node, &target->children_show ) ) {
			c = node->data;
			if( !c->computed_style.visible ) {
				continue;
			}
			if( LCUIRect_HasPoint(&c->box.border, x, y) ) {
				target = c;
				x -= c->box.padding.x;
				y -= c->box.padding.y;
				is_hit = TRUE;
				break;
			}
		}
	} while( is_hit );
	return (target == widget) ? NULL:target;
}

void Widget_GetAbsXY( LCUI_Widget w, LCUI_Widget parent, int *x, int *y )
{
	int tmp_x = 0, tmp_y = 0;
	while( w && w != parent ) {
		tmp_x += w->box.border.x;
		tmp_y += w->box.border.y;
		w = w->parent;
	}
	*x = tmp_x;
	*y = tmp_y;
}

LCUI_Widget LCUIWidget_GetById( const char *idstr )
{
	LCUI_Widget w;
	if( !idstr ) {
		return NULL;
	}
	LCUIMutex_Lock( &LCUIWidget.mutex );
	w = Dict_FetchValue( LCUIWidget.ids, idstr );
	LCUIMutex_Unlock( &LCUIWidget.mutex );
	return w;
}

LCUI_Widget Widget_GetPrev( LCUI_Widget w )
{
	LinkedListNode *node = Widget_GetNode( w );
	if( node->prev && node != w->parent->children.head.next ) {
		return node->prev->data;
	}
	return NULL;
}

LCUI_Widget Widget_GetNext( LCUI_Widget w )
{
	LinkedListNode *node = Widget_GetNode( w );
	if( node->next ) {
		return node->next->data;
	}
	return NULL;
}

int Widget_Top( LCUI_Widget w )
{
	DEBUG_MSG("tip\n");
	return Widget_Append( LCUIWidget.root, w );
}

void Widget_SetTitleW( LCUI_Widget w, const wchar_t *title )
{
	int len;
	wchar_t *new_title, *old_title;

	len = wcslen(title) + 1;
	new_title = (wchar_t*)malloc(sizeof(wchar_t)*len);
	if( !new_title ) {
		return;
	}
	wcsncpy( new_title, title, len );
	old_title = w->title;
	w->title = new_title;
	if( old_title ) {
		free( old_title );
	}
	Widget_AddTask( w, WTT_TITLE );
}

int Widget_SetId( LCUI_Widget w, const char *idstr )
{
	LCUIMutex_Lock( &LCUIWidget.mutex );
	if( w->id ) {
		Dict_Delete( LCUIWidget.ids, w->id );
		free( w->id );
		w->id = NULL;
	}
	if( !idstr ) {
		LCUIMutex_Unlock( &LCUIWidget.mutex );
		return -1;
	}
	w->id = strdup( idstr );
	if( Dict_Add( LCUIWidget.ids, w->id, w ) == 0 ) {
		LCUIMutex_Unlock( &LCUIWidget.mutex );
		return 0;
	}
	LCUIMutex_Unlock( &LCUIWidget.mutex );
	free( w->id );
	w->id = NULL;
	return -2;
}

static float ComputeXMetric( LCUI_Widget w, int key )
{
	LCUI_Style s = &w->style->sheet[key];
	if( s->type == SVT_SCALE ) {
		if( !w->parent ) {
			return 0;
		}
		if( w->computed_style.position == SV_ABSOLUTE ) {
			return w->parent->box.padding.width * s->scale;
		}
		return w->parent->box.content.width * s->scale;
	}
	return LCUIMetrics_ApplyDimension( s );
}

static float ComputeYMetric( LCUI_Widget w, int key )
{
	LCUI_Style s = &w->style->sheet[key];
	if( s->type == SVT_SCALE ) {
		if( !w->parent ) {
			return 0;
		}
		if( w->computed_style.position == SV_ABSOLUTE ) {
			return w->parent->box.padding.height * s->scale;
		}
		return w->parent->box.content.height * s->scale;
	}
	return LCUIMetrics_ApplyDimension( s );
}

static float ComputeSelfXMetric( LCUI_Widget w, int key )
{
	LCUI_Style s = &w->style->sheet[key];
	if( s->type == SVT_SCALE ) {
		return w->width * s->scale;
	}
	return LCUIMetrics_ApplyDimension( s );
}

static int ComputeStyleOption( LCUI_Widget w, int key, int default_value )
{
	if( !w->style->sheet[key].is_valid ) {
		return default_value;
	}
	if( w->style->sheet[key].type != SVT_STYLE ) {
		return default_value;
	}
	return w->style->sheet[key].style;
}

/** 计算边框样式 */
static void Widget_ComputeBorder( LCUI_Widget w )
{
	LCUI_Style style;
	LCUI_StyleSheet ss = w->style;
	LCUI_Border *b = &w->computed_style.border;
	int key = key_border_start ;
	unsigned int val;

	for( ; key <= key_border_end; ++key ) {
		style = &ss->sheet[key];
		if( !style->is_valid ) {
			continue;
		}
		switch( key ) {
		case key_border_top_color:
			b->top.color = style->color;
			break;
		case key_border_right_color:
			b->right.color = style->color;
			break;
		case key_border_bottom_color:
			b->bottom.color = style->color;
			break;
		case key_border_left_color:
			b->left.color = style->color;
			break;
		case key_border_top_width:
			b->top.width = (unsigned int)style->px;
			break;
		case key_border_right_width:
			b->right.width = (unsigned int)style->px;
			break;
		case key_border_bottom_width:
			b->bottom.width = (unsigned int)style->px;
			break;
		case key_border_left_width:
			b->left.width = (unsigned int)style->px;
			break;
		case key_border_top_style:
			b->top.style = style->value;
			break;
		case key_border_right_style:
			b->right.style = style->value;
			break;
		case key_border_bottom_style:
			b->bottom.style = style->value;
			break;
		case key_border_left_style:
			b->left.style = style->value;
			break;
		case key_border_top_left_radius:
			val = (unsigned int)ComputeSelfXMetric( w, key );
			b->top_left_radius = val;
			break;
		case key_border_top_right_radius:
			val = (unsigned int)ComputeSelfXMetric( w, key );
			b->top_right_radius = val;
			break;
		case key_border_bottom_left_radius:
			val = (unsigned int)ComputeSelfXMetric( w, key );
			b->bottom_left_radius = val;
			break;
		case key_border_bottom_right_radius:
			val = (unsigned int)ComputeSelfXMetric( w, key );
			b->bottom_right_radius = val;
			break;
		default: break;
		}
	}
}

void Widget_UpdateBorder( LCUI_Widget w )
{
	LCUI_Rect rect;
	LCUI_Border ob, *nb;
	ob = w->computed_style.border;
	Widget_ComputeBorder( w );
	nb = &w->computed_style.border;
	/* 如果边框变化并未导致图层尺寸变化的话，则只重绘边框 */
	if( ob.top.width != nb->top.width || 
	    ob.right.width != nb->right.width ||
	    ob.bottom.width != nb->bottom.width ||
	    ob.left.width != nb->left.width ) {
		Widget_AddTask( w, WTT_RESIZE );
		Widget_AddTask( w, WTT_POSITION );
		return;
	}
	rect.x = rect.y = 0;
	rect.width = w->box.border.width;
	rect.width -= max( ob.top_right_radius, ob.right.width );
	rect.height = max( ob.top_left_radius, ob.top.width );
	/* 上 */
	Widget_InvalidateArea( w, &rect, SV_BORDER_BOX );
	rect.x = w->box.border.width;
	rect.width = max( ob.top_right_radius, ob.right.width );
	rect.x -= rect.width;
	rect.height = w->box.border.height;
	rect.height -= max( ob.bottom_right_radius, ob.bottom.width );
	/* 右 */
	Widget_InvalidateArea( w, &rect, SV_BORDER_BOX );
	rect.x = max( ob.bottom_left_radius, ob.left.width );
	rect.y = w->box.border.height;
	rect.width = w->box.border.width;
	rect.width -= rect.x;
	rect.height = max( ob.bottom_right_radius, ob.bottom.width );
	rect.y -= rect.height;
	/* 下 */
	Widget_InvalidateArea( w, &rect, SV_BORDER_BOX );
	rect.width = rect.x;
	rect.x = 0;
	rect.y = max( ob.top_left_radius, ob.left.width );
	rect.height = w->box.border.height;
	rect.height -= rect.y;
	/* 左 */
	Widget_InvalidateArea( w, &rect, SV_BORDER_BOX );
}

/** 计算矩形阴影样式 */
static void ComputeBoxShadowStyle( LCUI_StyleSheet ss, LCUI_BoxShadow *bsd )
{
	LCUI_Style style;
	int key = key_box_shadow_start;
	memset( bsd, 0, sizeof( *bsd ) );
	for( ; key <= key_box_shadow_end; ++key ) {
		style = &ss->sheet[key];
		if( !style->is_valid ) {
			continue;
		}
		switch( key ) {
		case key_box_shadow_x:
			bsd->x = (int)style->px;
			break;
		case key_box_shadow_y:
			bsd->y = (int)style->px;
			break;
		case key_box_shadow_spread:
			bsd->spread = (int)style->px;
			break;
		case key_box_shadow_blur:
			bsd->blur = (int)style->px;
			break;
		case key_box_shadow_color:
			bsd->color = style->color;
			break;
		default: break;
		}
	}
}

void Widget_UpdateBoxShadow( LCUI_Widget w )
{
	LCUI_BoxShadow bs = w->computed_style.shadow;
	ComputeBoxShadowStyle( w->style, &w->computed_style.shadow );
	/* 如果阴影变化并未导致图层尺寸变化，则只重绘阴影 */
	if( bs.x == w->computed_style.shadow.x &&
	    bs.y == w->computed_style.shadow.y &&
	    bs.blur == w->computed_style.shadow.blur ) {
		int i;
		LCUI_Rect rects[4], rb, rg;
		RectF2Rect( w->box.border, rb );
		RectF2Rect( w->box.graph, rg );
		LCUIRect_CutFourRect( &rb, &rg, rects );
		for( i = 0; i < 4; ++i ) {
			rects[i].x -= w->box.graph.x;
			rects[i].y -= w->box.graph.y;
			Widget_InvalidateArea( w, &rects[i], SV_GRAPH_BOX );
		}
		return;
	}
	Widget_AddTask( w, WTT_RESIZE );
	Widget_AddTask( w, WTT_POSITION );
}

void Widget_UpdateVisibility( LCUI_Widget w )
{
	int display = w->computed_style.display;
	LCUI_Style s = &w->style->sheet[key_visible];
	LCUI_BOOL visible = w->computed_style.visible;
	if( w->computed_style.display == SV_NONE ) {
		visible = FALSE;
	}
	w->computed_style.visible = !(s->is_valid && !s->value);
	s = &w->style->sheet[key_display];
	if( s->is_valid ) {
		w->computed_style.display = s->style;
		if( w->computed_style.display == SV_NONE ) {
			w->computed_style.visible = FALSE;
		}
	} else {
		w->computed_style.display = SV_BLOCK;
	}
	if( visible == w->computed_style.visible ) {
		return;
	}
	visible = w->computed_style.visible;
	if( w->parent ) {
		Widget_PushInvalidArea( w, NULL, SV_GRAPH_BOX );
		if( w->computed_style.display != display ||
		    w->computed_style.position != SV_ABSOLUTE ) {
			Widget_UpdateLayout( w->parent );
		}
	}
	DEBUG_MSG( "visible: %s\n", visible ? "TRUE" : "FALSE" );
	Widget_PostSurfaceEvent( w, visible ? WET_SHOW : WET_HIDE );
}

void Widget_UpdateOpacity( LCUI_Widget w )
{
	float opacity = 1.0;
	LCUI_Style s = &w->style->sheet[key_opacity];
	if( s->is_valid ) {
		switch( s->type ) {
		case SVT_VALUE: opacity = 1.0 * s->value; break;
		case SVT_SCALE: opacity = s->val_scale; break;
		default: opacity = 1.0; break;
		}
		if( opacity > 1.0 ) {
			opacity = 1.0;
		} else if( opacity < 0.0 ) {
			opacity = 0.0;
		}
	}
	w->computed_style.opacity = opacity;
	Widget_InvalidateArea( w, NULL, SV_GRAPH_BOX );
	DEBUG_MSG("opacity: %0.2f\n", opacity);
}

void Widget_UpdateZIndex( LCUI_Widget w )
{
	Widget_AddTask( w, WTT_ZINDEX );
}

void Widget_ExecUpdateZIndex( LCUI_Widget w )
{
	int z_index;
	LinkedList *list;
	LinkedListNode *cnode, *csnode, *snode;
	LCUI_Style s = &w->style->sheet[key_z_index];
	if( s->is_valid && s->type == SVT_VALUE ) {
		z_index = s->value;
	} else {
		z_index = 0;
	}
	if( !w->parent ) {
		return;
	}
	if( w->state == WSTATE_NORMAL ) {
		if( w->computed_style.z_index == z_index ) {
			return;
		}
	}
	w->computed_style.z_index = z_index;
	snode = Widget_GetShowNode( w );
	list = &w->parent->children_show;
	LinkedList_Unlink( list, snode );
	for( LinkedList_Each( cnode, list ) ) {
		LCUI_Widget child = cnode->data;
		LCUI_WidgetStyle *ccs = &child->computed_style;
		csnode = Widget_GetShowNode( child );
		if( w->computed_style.z_index < ccs->z_index ) {
			continue;
		} else if( w->computed_style.z_index == ccs->z_index ) {
			if( w->computed_style.position == ccs->position ) {
				if( w->index < child->index ) {
					continue;
				}
			} else if( w->computed_style.position < ccs->position ) {
				continue;
			}
		}
		LinkedList_Link( list, csnode->prev, snode );
		break;
	}
	if( !cnode ) {
		LinkedList_AppendNode( list, snode );
	}
	if( w->computed_style.position != SV_STATIC ) {
		Widget_AddTask( w, WTT_REFRESH );
	}
}

/** 清除已计算的尺寸 */
static void Widget_ClearComputedSize( LCUI_Widget w )
{
	LCUI_Style sw = &w->style->sheet[key_width];
	LCUI_Style sh = &w->style->sheet[key_height];
	if( !sw->is_valid || (sw->is_valid && sw->type == SVT_AUTO) ) {
		w->width = 0;
		w->box.content.width = 0;
	}
	if( !sh->is_valid || (sh->is_valid && sh->type == SVT_AUTO) ) {
		w->height = 0;
		w->box.content.height = 0;
	}
}

static void Widget_UpdateChildrenSize( LCUI_Widget w )
{
	LinkedListNode *node;
	for( LinkedList_Each( node, &w->children ) ) {
		LCUI_Widget child = node->data;
		LCUI_Style s = child->style->sheet;
		if( child->computed_style.display == SV_BLOCK ) {
			if( CheckStyleType( s, key_width, AUTO ) ||
			    CheckStyleType( s, key_height, AUTO ) ) {
				Widget_AddTask( child, WTT_RESIZE );
			}
		}
		if( CheckStyleType( s, key_width, SCALE ) ||
		    CheckStyleType( s, key_height, SCALE ) ) {
			Widget_AddTask( child, WTT_RESIZE );
		}
		if( child->computed_style.position == SV_ABSOLUTE ) {
			if( s[key_right].is_valid || s[key_bottom].is_valid ||
			    CheckStyleType( s, key_left, scale ) ||
			    CheckStyleType( s, key_top, scale ) ) {
				Widget_AddTask( child, WTT_POSITION );
			}
		}
		if( CheckStyleValue( s, key_margin_left, AUTO ) ||
		    CheckStyleValue( s, key_margin_right, AUTO ) ) {
			Widget_AddTask( child, WTT_MARGIN );
		}
		if( child->computed_style.vertical_align != SV_TOP ) {
			Widget_AddTask( child, WTT_POSITION );
		}
	}
}

void Widget_UpdatePosition( LCUI_Widget w )
{
	LCUI_Rect rect;
	int position = ComputeStyleOption( w, key_position, SV_STATIC );
	int valign = ComputeStyleOption( w, key_vertical_align, SV_TOP );
	w->computed_style.vertical_align = valign;
	w->computed_style.left = ComputeXMetric( w, key_left );
	w->computed_style.right = ComputeXMetric( w, key_right );
	w->computed_style.top = ComputeYMetric( w, key_top );
	w->computed_style.bottom = ComputeYMetric( w, key_bottom );
	if( w->parent && w->computed_style.position != position ) {
		w->computed_style.position = position;
		Widget_UpdateLayout( w->parent );
		Widget_ClearComputedSize( w );
		Widget_UpdateChildrenSize( w );
		/* 当部件尺寸是按百分比动态计算的时候需要重新计算尺寸 */
		if( CheckStyleType( w->style->sheet, key_width, scale ) ||
		    CheckStyleType( w->style->sheet, key_height, scale ) ) {
			Widget_UpdateSize( w );
		}
	}
	w->computed_style.position = position;
	RectF2Rect( w->box.graph, rect );
	Widget_UpdateZIndex( w );
	w->x = w->origin_x;
	w->y = w->origin_y;
	switch( position ) {
	case SV_ABSOLUTE:
		w->x = w->y = 0;
		if( w->style->sheet[key_left].is_valid ) {
			w->x = w->computed_style.left;
		} else if( w->style->sheet[key_right].is_valid ) {
			if( w->parent ) {
				w->x = w->parent->box.border.width;
				w->x -= w->width;
			}
			w->x -= w->computed_style.right;
		}
		if( w->style->sheet[key_top].is_valid ) {
			w->y = w->computed_style.top;
		} else if( w->style->sheet[key_bottom].is_valid ) {
			if( w->parent ) {
				w->y = w->parent->box.border.height;
				w->y -= w->height;
			}
			w->y -= w->computed_style.bottom;
		}
		break;
	case SV_RELATIVE:
		if( w->style->sheet[key_left].is_valid ) {
			w->x -= w->computed_style.left;
		} else if( w->style->sheet[key_right].is_valid ) {
			w->x += w->computed_style.right;
		}
		if( w->style->sheet[key_top].is_valid ) {
			w->y += w->computed_style.top;
		} else if( w->style->sheet[key_bottom].is_valid ) {
			w->y -= w->computed_style.bottom;
		}
	default:
		if( w->parent ) {
			w->x += w->parent->padding.left;
			w->y += w->parent->padding.top;
		}
		break;
	}
	switch( valign ) {
	case SV_MIDDLE:
		if( !w->parent ) {
			break;
		}
		w->y += (w->parent->box.content.height - w->height) / 2;
		break;
	case SV_BOTTOM:
		if( !w->parent ) {
			break;
		}
		w->y += w->parent->box.content.height - w->height;
	case SV_TOP:
	default: break;
	}
	w->box.outer.x = w->x;
	w->box.outer.y = w->y;
	w->x += w->margin.left;
	w->y += w->margin.top;
	/* 以x、y为基础 */
	w->box.padding.x = w->x;
	w->box.padding.y = w->y;
	w->box.border.x = w->x;
	w->box.border.y = w->y;
	w->box.graph.x = w->x;
	w->box.graph.y = w->y;
	/* 计算各个框的坐标 */
	w->box.padding.x += w->computed_style.border.left.width;
	w->box.padding.y += w->computed_style.border.top.width;
	w->box.content.x = w->box.padding.x + w->padding.left;
	w->box.content.y = w->box.padding.y + w->padding.top;
	w->box.graph.x -= BoxShadow_GetBoxX( &w->computed_style.shadow );
	w->box.graph.y -= BoxShadow_GetBoxY( &w->computed_style.shadow );
	if( w->parent ) {
		DEBUG_MSG("new-rect: %d,%d,%d,%d\n", w->box.graph.x, w->box.graph.y, w->box.graph.w, w->box.graph.h);
		DEBUG_MSG("old-rect: %d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height);
		/* 标记移动前后的区域 */
		Widget_PushInvalidArea( w, NULL, SV_GRAPH_BOX );
		Widget_PushInvalidArea( w->parent, &rect, SV_PADDING_BOX );
	}
	/* 检测是否为顶级部件并做相应处理 */
	Widget_PostSurfaceEvent( w, WET_MOVE );
}

/** 更新位图尺寸 */
static void Widget_UpdateGraphBox( LCUI_Widget w )
{
	LCUI_RectF *rb = &w->box.border;
	LCUI_RectF *rg = &w->box.graph;
	LCUI_BoxShadow *shadow = &w->computed_style.shadow;
	rg->x = w->x - BoxShadow_GetBoxX( shadow );
	rg->y = w->y - BoxShadow_GetBoxY( shadow );
	rg->width = BoxShadow_GetWidth( shadow, rb->width );
	rg->height = BoxShadow_GetHeight( shadow, rb->height );
	if( !w->enable_graph ) {
		Graph_Free( &w->graph );
		return;
	}
	/* 如果有会产生透明效果的样式 */
	if( w->computed_style.border.bottom_left_radius > 0
	    || w->computed_style.border.bottom_right_radius > 0
	    || w->computed_style.border.top_left_radius > 0
	    || w->computed_style.border.top_right_radius > 0
	    || w->computed_style.background.color.alpha < 255
	    || w->computed_style.shadow.blur > 0 ) {
		w->graph.color_type = COLOR_TYPE_ARGB;
	} else {
		w->graph.color_type = COLOR_TYPE_RGB;
	}
	Graph_Create( &w->graph, rg->width, rg->height );
}

/** 计算合适的内容框大小 */
static void Widget_ComputeContentSize( LCUI_Widget w,
				       float *width, float *height )
{
	float n;
	LCUI_Style s;
	LinkedListNode *node;

	*width = *height = 0.0;
	for( LinkedList_Each( node, &w->children_show ) ) {
		LCUI_Widget child = node->data;
		LCUI_WidgetBoxRect *box = &child->box;
		LCUI_WidgetStyle *style = &child->computed_style;
		/* 忽略不可见、绝对定位的部件 */
		if( !style->visible || style->position == SV_ABSOLUTE ) {
			continue;
		}
		s = &child->style->sheet[key_width];
		/* 对于宽度以百分比做单位的，计算尺寸时自动去除外间距框、
		 * 内间距框和边框占用的空间
		 */
		if( s->type == SVT_SCALE ) {
			if( style->box_sizing == SV_BORDER_BOX ) {
				n = box->border.x + box->border.width;
			} else {
				n = box->content.x + box->content.width;
				n -= box->content.x - box->border.x;
			}
			n -= box->outer.x - box->border.x;
		} else if( box->outer.width <= 0 ) {
			continue;
		} else {
			n = box->outer.x + box->outer.width;
		}
		if( n > *width ) {
			*width = n;
		}
		s = &child->style->sheet[key_height];
		n = box->outer.y + box->outer.height;
		if( s->type == SVT_SCALE ) {
			if( style->box_sizing == SV_BORDER_BOX ) {
				n = box->border.y + box->border.height;
			} else {
				n = box->content.y + box->content.height;
				n -= box->content.y - box->border.y;
			}
			n -= box->outer.y - box->border.y;
		} else if( box->outer.height <= 0 ) {
			continue;
		} else {
			n = box->outer.y + box->outer.height;
		}
		if( n > *height ) {
			*height = n;
		}
	}
	/* 计算出来的尺寸是包含 padding-left 和 padding-top 的，因此需要减去它们 */
	*width -= w->padding.left;
	*height -= w->padding.top;
}

/** 计算尺寸 */
static void Widget_ComputeSize( LCUI_Widget w )
{
	float width, height;
	LCUI_RectF *box, *pbox = &w->box.padding;
	LCUI_WidgetStyle *style = &w->computed_style;
	LCUI_Style sw = &w->style->sheet[key_width];
	LCUI_Style sh = &w->style->sheet[key_height];
	LCUI_Border *bbox = &style->border;
	w->width = ComputeXMetric( w, key_width );
	w->height = ComputeYMetric( w, key_height );
	if( sw->type == SVT_AUTO || sh->type == SVT_AUTO ) {
		if( w->proto && w->proto->autosize ) {
			w->proto->autosize( w, &width, &height );
		} else {
			Widget_ComputeContentSize( w, &width, &height );
		}
		/* 以上计算出来的是内容框尺寸，如果尺寸调整模式是基于边框盒，则
		 * 转换为边框盒尺寸
		 */
		if( w->computed_style.box_sizing == SV_BORDER_BOX ) {
			width += w->padding.left + w->padding.right;
			width += bbox->left.width + bbox->right.width;
			height += w->padding.top + w->padding.bottom;
			height += bbox->top.width + bbox->bottom.width;
		}
		if( w->parent && sw->type == SVT_AUTO &&
		    w->computed_style.display == SV_BLOCK &&
		    w->computed_style.position != SV_ABSOLUTE ) {
			width = w->parent->box.content.width;
			width -= w->margin.left + w->margin.right;
			if( w->computed_style.box_sizing != SV_BORDER_BOX ) {
				width -= w->padding.left + w->padding.right;
				/* 边框的宽度为整数，不使用浮点数 */
				width -= bbox->left.width * 1.0;
				width -= bbox->right.width * 1.0;
			}
		}
		if( sw->type == SVT_AUTO ) {
			w->width = width;
		}
		if( sh->type == SVT_AUTO ) {
			w->height = height;
		}
	}
	while( sw->type == SVT_SCALE && w->parent ) {
		LCUI_Style psw = &w->parent->style->sheet[key_width];
		if( psw->type != SVT_AUTO ) {
			break;
		}
		if( w->proto && w->proto->autosize ) {
			w->proto->autosize( w, &width, &height );
		} else {
			Widget_ComputeContentSize( w, &width, &height );
		}
		if( w->computed_style.box_sizing == SV_BORDER_BOX ) {
			width += w->padding.left + w->padding.right;
			width += bbox->left.width + bbox->right.width;
			height += w->padding.top + w->padding.bottom;
			height += bbox->top.width + bbox->bottom.width;
		}
		if( width > w->width ) {
			w->width = width;
			w->height = height;
			Widget_AddTask( w->parent, WTT_RESIZE );
		}
		break;
	}
	if( w->style->sheet[key_max_width].is_valid ) {
		style->max_width = ComputeXMetric( w, key_max_width );
	} else {
		style->max_width = -1;
	}
	if( w->style->sheet[key_min_width].is_valid ) {
		style->min_width = ComputeXMetric( w, key_min_width );
	} else {
		style->min_width = -1;
	}
	if( w->style->sheet[key_max_height].is_valid ) {
		style->max_height = ComputeXMetric( w, key_max_height );
	} else {
		style->max_height = -1;
	}
	if( w->style->sheet[key_min_height].is_valid ) {
		style->min_height = ComputeXMetric( w, key_min_height );
	} else {
		style->min_height = -1;
	}
	if( style->max_width > -1 && w->width > style->max_width ) {
		w->width = style->max_width;
	}
	if( style->max_height > -1 && w->height > style->max_height ) {
		w->height = style->max_height;
	}
	if( w->width < style->min_width ) {
		w->width = style->min_width;
	}
	if( w->height < style->min_height ) {
		w->height = style->min_height;
	}
	w->box.border.width = w->width;
	w->box.border.height = w->height;
	w->box.content.width = w->width;
	w->box.content.height = w->height;
	w->box.padding.width = w->width;
	w->box.padding.height = w->height;
	/* 如果是以边框盒作为尺寸调整对象，则需根据边框盒计算内容框尺寸 */
	if( w->computed_style.box_sizing == SV_BORDER_BOX ) {
		box = &w->box.content;
		pbox->width -= bbox->left.width + bbox->right.width;
		pbox->height -= bbox->top.width + bbox->bottom.width;
		box->width = pbox->width;
		box->height = pbox->height;
		box->width -= w->padding.left + w->padding.right;
		box->height -= w->padding.top + w->padding.bottom;
	} else {
		/* 否则是以内容框作为尺寸调整对象，需计算边框盒的尺寸 */
		box = &w->box.border;
		pbox->width += w->padding.left + w->padding.right;
		pbox->height += w->padding.top + w->padding.bottom;
		box->width = pbox->width;
		box->height = pbox->height;
		box->width += bbox->left.width + bbox->right.width;
		box->height += bbox->top.width + bbox->bottom.width;
	}
	w->width = w->box.border.width;
	w->height = w->box.border.height;
	w->box.outer.width = w->box.border.width;
	w->box.outer.height = w->box.border.height;
	w->box.outer.width += w->margin.left + w->margin.right;
	w->box.outer.height += w->margin.top + w->margin.bottom;
}

static void Widget_SendResizeEvent( LCUI_Widget w )
{
	LCUI_WidgetEventRec e;
	e.target = w;
	e.data = NULL;
	e.type = WET_RESIZE;
	e.cancel_bubble = TRUE;
	Widget_TriggerEvent( w, &e, NULL );
	Widget_AddTask( w, WTT_REFRESH );
	Widget_PostSurfaceEvent( w, WET_RESIZE );
}

void Widget_UpdateMargin( LCUI_Widget w )
{
	int i;
	LCUI_BoundBox *mbox = &w->computed_style.margin;
	struct { 
		LCUI_Style sval;
		float *fval;
		int key;
	} pd_map[4] = {
		{ &mbox->top, &w->margin.top, key_margin_top },
		{ &mbox->right, &w->margin.right, key_margin_right },
		{ &mbox->bottom, &w->margin.bottom, key_margin_bottom },
		{ &mbox->left, &w->margin.left, key_margin_left }
	};
	for( i = 0; i < 4; ++i ) {
		LCUI_Style s = &w->style->sheet[pd_map[i].key];
		if( !s->is_valid ) {
			pd_map[i].sval->type = SVT_PX;
			pd_map[i].sval->px = 0.0;
			*pd_map[i].fval = 0.0;
			continue;
		}
		*pd_map[i].sval = *s;
		*pd_map[i].fval = LCUIMetrics_ApplyDimension( s );;
	}
	/* 如果有父级部件，则处理 margin-left 和 margin-right 的值 */
	if( w->parent ) {
		int width = w->parent->box.content.width;
		int margin_left = SVT_AUTO, margin_right = SVT_AUTO;
		if( w->style->sheet[key_margin_left].is_valid ) {
			margin_left = w->style->sheet[key_margin_left].type;
		}
		if( w->style->sheet[key_margin_right].is_valid ) {
			margin_right = w->style->sheet[key_margin_right].type;
		}
		if( margin_left == SVT_AUTO ) {
			if( margin_right == SVT_AUTO ) {
				w->margin.left = (width - w->width) / 2;
				if( w->margin.left < 0 ) {
					w->margin.left = 0;
				}
				w->margin.right = w->margin.left;
			} else {
				w->margin.left = width - w->width;
				w->margin.left -= w->margin.right;
				if( w->margin.left < 0 ) {
					w->margin.left = 0;
				}
			}
		} else if( margin_right == SVT_AUTO ) {
			w->margin.right = width - w->width;
			w->margin.right -= w->margin.left;
			if( w->margin.right < 0 ) {
				w->margin.right = 0;
			}
		}
	}
	if( w->parent ) {
		if( w->parent->style->sheet[key_width].type == SVT_AUTO ||
		    w->parent->style->sheet[key_height].type == SVT_AUTO ) {
			Widget_AddTask( w->parent, WTT_RESIZE );
		}
		if( w->computed_style.display != SV_NONE &&
		    w->computed_style.position == SV_STATIC ) {
			Widget_UpdateLayout( w->parent );
		}
	}
	Widget_AddTask( w, WTT_POSITION );
}

void Widget_UpdateSize( LCUI_Widget w )
{
	LCUI_RectF rect;
	int i, box_sizing;
	LCUI_Rect2F padding = w->padding;
	LCUI_BoundBox *pbox = &w->computed_style.padding;
	struct {
		LCUI_Style sval;
		float *ival;
		int key;
	} pd_map[4] = {
		{ &pbox->top, &w->padding.top, key_padding_top },
		{ &pbox->right, &w->padding.right, key_padding_right },
		{ &pbox->bottom, &w->padding.bottom, key_padding_bottom },
		{ &pbox->left, &w->padding.left, key_padding_left }
	};
	rect = w->box.graph;
	/* 内边距的单位暂时都用 px  */
	for( i = 0; i < 4; ++i ) {
		LCUI_Style s = &w->style->sheet[pd_map[i].key];
		if( !s->is_valid ) {
			pd_map[i].sval->type = SVT_PX;
			pd_map[i].sval->px = 0.0;
			*pd_map[i].ival = 0.0;
			continue;
		}
		*pd_map[i].sval = *s;
		*pd_map[i].ival = LCUIMetrics_ApplyDimension( s );
	}
	box_sizing = ComputeStyleOption( w, key_box_sizing, SV_CONTENT_BOX );
	w->computed_style.box_sizing = box_sizing;
	Widget_ComputeSize( w );
	Widget_UpdateGraphBox( w );
	/* 如果左右外间距是 auto 类型的，则需要计算外间距 */
	if( w->style->sheet[key_margin_left].is_valid &&
	    w->style->sheet[key_margin_left].type == SVT_AUTO ) {
		Widget_UpdateMargin( w );
	} else if( w->style->sheet[key_margin_right].is_valid &&
		   w->style->sheet[key_margin_right].type == SVT_AUTO ) {
		Widget_UpdateMargin( w );
	}
	/* 若尺寸无变化则不继续处理 */
	if( rect.width == w->box.graph.width &&
	    rect.height == w->box.graph.height && 
	    padding.top == w->padding.top &&
	    padding.right == w->padding.right &&
	    padding.bottom == w->padding.bottom &&
	    padding.left == w->padding.left ) {
		return;
	}
	/* 若在变化前后的宽高中至少有一个为 0，则不继续处理 */
	if( (w->box.graph.width <= 0 || w->box.graph.height <= 0) &&
	    (rect.width <= 0 || rect.height <= 0) ) {
		return;
	}
	if( w->style->sheet[key_height].type != SVT_AUTO ) {
		Widget_UpdateLayout( w );
	}
	/* 如果垂直对齐方式不为顶部对齐 */
	if( w->computed_style.vertical_align != SV_TOP ) {
		Widget_UpdatePosition( w );
	} else if( w->computed_style.position == SV_ABSOLUTE ) {
		/* 如果是绝对定位，且指定了右间距或底间距 */
		if( !CheckStyleValue( w->style->sheet, key_right, AUTO ) ||
		    !CheckStyleValue( w->style->sheet, key_bottom, AUTO ) ) {
			Widget_UpdatePosition( w );
		}
	}
	if( w->parent ) {
		LCUI_Rect r;
		RectF2Rect( rect, r );
		Widget_InvalidateArea( w->parent, &r, SV_PADDING_BOX );
		r.width = roundi( w->box.graph.width );
		r.height = roundi( w->box.graph.height );
		Widget_InvalidateArea( w->parent, &r, SV_PADDING_BOX );
		if( w->parent->style->sheet[key_width].type == SVT_AUTO
		    || w->parent->style->sheet[key_height].type == SVT_AUTO ) {
			Widget_AddTask( w->parent, WTT_RESIZE );
		}
		if( w->computed_style.display != SV_NONE &&
		    w->computed_style.position == SV_STATIC ) {
			Widget_UpdateLayout( w->parent );
		}
	}
	Widget_SendResizeEvent( w );
	Widget_UpdateChildrenSize( w );
}

void Widget_UpdateProps( LCUI_Widget w )
{
	LCUI_Style s;
	int prop = ComputeStyleOption( w, key_pointer_events, SV_AUTO );
	w->computed_style.pointer_events = prop;
	s = &w->style->sheet[key_focusable];
	if( s->is_valid && s->type == SVT_BOOL && s->value == 0 ) {
		w->computed_style.focusable = FALSE;
	} else {
		w->computed_style.focusable = TRUE;
	}
}

void Widget_SetBorder( LCUI_Widget w, int width, int style, LCUI_Color clr )
{
	Widget_SetStyle( w, key_border_top_color, clr, color );
	Widget_SetStyle( w, key_border_right_color, clr, color );
	Widget_SetStyle( w, key_border_bottom_color, clr, color );
	Widget_SetStyle( w, key_border_left_color, clr, color );
	Widget_SetStyle( w, key_border_top_width, width, px );
	Widget_SetStyle( w, key_border_right_width, width, px );
	Widget_SetStyle( w, key_border_bottom_width, width, px );
	Widget_SetStyle( w, key_border_left_width, width, px );
	Widget_SetStyle( w, key_border_top_style, style, style );
	Widget_SetStyle( w, key_border_right_style, style, style );
	Widget_SetStyle( w, key_border_bottom_style, style, style );
	Widget_SetStyle( w, key_border_left_style, style, style );
	Widget_UpdateStyle( w, FALSE );
}

void Widget_SetPadding( LCUI_Widget w, float top, float right, float bottom, float left )
{
	Widget_SetStyle( w, key_padding_top, top, px );
	Widget_SetStyle( w, key_padding_right, right, px );
	Widget_SetStyle( w, key_padding_bottom, bottom, px );
	Widget_SetStyle( w, key_padding_left, left, px );
	Widget_UpdateStyle( w, FALSE );
}

void Widget_SetMargin( LCUI_Widget w, float top, float right, float bottom, float left )
{
	Widget_SetStyle( w, key_margin_top, top, px );
	Widget_SetStyle( w, key_margin_right, right, px );
	Widget_SetStyle( w, key_margin_bottom, bottom, px );
	Widget_SetStyle( w, key_margin_left, left, px );
	Widget_UpdateStyle( w, FALSE );
}

void Widget_Move( LCUI_Widget w, float left, float top )
{
	SetStyle( w->custom_style, key_top, top, px );
	SetStyle( w->custom_style, key_left, left, px );
	DEBUG_MSG("top = %d, left = %d\n", top, left);
	Widget_UpdateStyle( w, FALSE );
}

void Widget_Resize( LCUI_Widget w, float width, float height )
{
	SetStyle( w->custom_style, key_width, width, px );
	SetStyle( w->custom_style, key_height, height, px );
	Widget_UpdateStyle( w, FALSE );
}

void Widget_Show( LCUI_Widget w )
{
	SetStyle( w->custom_style, key_visible, TRUE, int );
	Widget_UpdateStyle( w, FALSE );
}

void Widget_Hide( LCUI_Widget w )
{
	SetStyle( w->custom_style, key_visible, FALSE, int );
	Widget_UpdateStyle( w, FALSE );
}

void Widget_SetBackgroundColor( LCUI_Widget w, LCUI_Color color )
{
	w->computed_style.background.color = color;
}

void Widget_SetDisabled( LCUI_Widget w, LCUI_BOOL disabled )
{
	w->disabled = disabled;
	if( w->disabled ) {
		Widget_AddStatus( w, "disabled" );
	} else {
		Widget_RemoveStatus( w, "disabled" );
	}
}

int Widget_SetAttributeEx( LCUI_Widget w, const char *name, void *value,
			   int value_type, void( *value_destructor )(void*) )
{
	LCUI_WidgetAttribute attr;
	if( !w->attributes ) {
		w->attributes = Dict_Create( &LCUIWidget.dt_attributes, NULL );
	}
	attr = Dict_FetchValue( w->attributes, name );
	if( attr ) {
		if( attr->value.destructor ) {
			attr->value.destructor( attr->value.data );
		}
	} else {
		attr = NEW( LCUI_WidgetAttributeRec, 1 );
		attr->name = strdup( name );
		Dict_Add( w->attributes, attr->name, attr );
	}
	attr->value.type = value_type;
	attr->value.string = strdup( value );
	attr->value.destructor = value_destructor;
	return 0;
}

int Widget_SetAttribute( LCUI_Widget w, const char *name, const char *value )
{
	char *value_str;
	if( !value ) {
		return Widget_SetAttributeEx( w, name, NULL, SVT_NONE, NULL );
	}
	value_str = strdup( value );
	if( !value_str ) {
		return -ENOMEM;
	}
	return Widget_SetAttributeEx( w, name, value_str, SVT_STRING, free );
}

const char *Widget_GetAttribute( LCUI_Widget w, const char *name )
{
	LCUI_WidgetAttribute attr;
	if( !w->attributes ) {
		return NULL;
	}
	attr = Dict_FetchValue( w->attributes, name );
	if( attr ) {
		return attr->value.string;
	}
	return NULL;
}

LCUI_BOOL Widget_CheckType( LCUI_Widget w, const char *type )
{
	LCUI_WidgetPrototypeC proto;

	if( ! w || !w->type ) {
		return FALSE;
	}
	if( strcmp( w->type, type ) == 0 ) {
		return TRUE;
	}
	for( proto = w->proto->proto; proto; proto = proto->proto ) {
		if( strcmp( proto->name, type ) == 0 ) {
			return TRUE;
		}
	}
	return FALSE;
}

LCUI_BOOL Widget_CheckPrototype( LCUI_Widget w, LCUI_WidgetPrototypeC proto )
{
	LCUI_WidgetPrototypeC p;
	for( p = w->proto; p; p = p->proto ) {
		if( p == proto ) {
			return TRUE;
		}
	}
	return FALSE;
}

/** 为部件添加一个类 */
int Widget_AddClass( LCUI_Widget w, const char *class_name )
{
	if( strshas( w->classes, class_name ) ) {
		return 1;
	}
	if( strsadd( &w->classes, class_name ) <= 0 ) {
		return 0;
	}
	Widget_HandleChildrenStyleChange( w, 0, class_name );
	Widget_UpdateStyle( w, TRUE );
	return 1;
}

/** 判断部件是否包含指定的类 */
LCUI_BOOL Widget_HasClass( LCUI_Widget w, const char *class_name )
{
	if( strshas( w->classes, class_name ) ) {
		return TRUE;
	}
	return FALSE;
}

/** 从部件中移除一个类 */
int Widget_RemoveClass( LCUI_Widget w, const char *class_name )
{
	if( strshas( w->classes, class_name ) ) {
		Widget_HandleChildrenStyleChange( w, 0, class_name );
		strsdel( &w->classes, class_name );
		Widget_UpdateStyle( w, TRUE );
		return 1;
	}
	return 0;
}

int Widget_AddStatus( LCUI_Widget w, const char *status_name )
{
	if( strshas( w->status, status_name ) ) {
		return 0;
	}
	if( strsadd( &w->status, status_name ) <= 0 ) {
		return 0;
	}
	Widget_HandleChildrenStyleChange( w, 1, status_name );
	Widget_UpdateStyle( w, TRUE );
	return 1;
}

LCUI_BOOL Widget_HasStatus( LCUI_Widget w, const char *status_name )
{
	if( strshas( w->status, status_name ) ) {
		return TRUE;
	}
	return FALSE;
}

int Widget_RemoveStatus( LCUI_Widget w, const char *status_name )
{
	if( strshas( w->status, status_name ) ) {
		Widget_HandleChildrenStyleChange( w, 1, status_name );
		strsdel( &w->status, status_name );
		Widget_UpdateStyle( w, TRUE );
		return 1;
	}
	return 0;
}

float Widget_ComputeMaxWidth( LCUI_Widget w )
{
	LCUI_Style s;
	LCUI_Widget child;
	float scale = 1.0;
	float width, padding = 0;
	width = LCUIWidget.root->box.padding.width;
	for( child = w; child->parent; child = child->parent ) {
		s = &child->style->sheet[key_width];
		switch( s->type ) {
		case SVT_PX:
			width = s->val_px;
			break;
		case SVT_SCALE:
			scale *= s->val_scale;
			if( child == w ) {
				break;
			}
			padding += child->padding.left;
			padding += child->padding.right;
		case SVT_AUTO:
		default: continue;
		}
		break;
	}
	width = scale * width;
	width -= padding;
	if( width < 0 ) {
		width = 0;
	}
	return width;
}

void Widget_LockLayout( LCUI_Widget w )
{
	w->layout_locked = TRUE;
}

void Widget_UnlockLayout( LCUI_Widget w )
{
	w->layout_locked = FALSE;
}

void Widget_UpdateLayout( LCUI_Widget w )
{
	if( !w->layout_locked ) {
		Widget_AddTask( w, WTT_LAYOUT );
	}
}

void Widget_ExecUpdateLayout( LCUI_Widget w )
{
	struct {
		float x, y;
		float line_height;
		LCUI_Widget prev;
		int prev_display;
		float max_width;
	} ctx = { 0 };
	LCUI_Widget child;
	LCUI_WidgetEventRec e = { 0 };
	LinkedListNode *node;

	ctx.max_width = Widget_ComputeMaxWidth( w );
	for( LinkedList_Each( node, &w->children ) ) {
		child = node->data;
		if( child->computed_style.position != SV_STATIC &&
		    child->computed_style.position != SV_RELATIVE ) {
			/* 如果部件还处于未准备完毕的状态 */
			if( child->state < WSTATE_READY ) {
				child->state |= WSTATE_LAYOUTED;
				/* 如果部件已经准备完毕则触发 ready 事件 */
				if( child->state == WSTATE_READY ) {
					LCUI_WidgetEventRec e;
					e.type = WET_READY;
					e.cancel_bubble = TRUE;
					Widget_TriggerEvent( child, &e, NULL );
					child->state = WSTATE_NORMAL;
				}
			}
			continue;
		}
		switch( child->computed_style.display ) {
		case SV_BLOCK:
			ctx.x = 0;
			if( ctx.prev && ctx.prev_display != SV_BLOCK ) {
				ctx.y += ctx.line_height;
			}
			child->origin_x = ctx.x;
			child->origin_y = ctx.y;
			ctx.line_height = child->box.outer.height;
			ctx.y += child->box.outer.height;
			break;
		case SV_INLINE_BLOCK:
			if( ctx.prev && ctx.prev_display == SV_BLOCK ) {
				ctx.x = 0;
				ctx.line_height = 0;
			}
			child->origin_x = ctx.x;
			ctx.x += child->box.outer.width;
			/* 只考虑小数点后两位 */
			if( ctx.x - ctx.max_width >= 0.01 ) {
				child->origin_x = 0;
				ctx.y += ctx.line_height;
				ctx.x = child->box.outer.width;
			}
			child->origin_y = ctx.y;
			if( child->box.outer.height > ctx.line_height ) {
				ctx.line_height = child->box.outer.height;
			}
			break;
		case SV_NONE:
		default: continue;
		}
		Widget_UpdatePosition( child );
		if( child->state < WSTATE_READY ) {
			child->state |= WSTATE_LAYOUTED;
			if( child->state == WSTATE_READY ) {
				LCUI_WidgetEventRec e;
				e.type = WET_READY;
				e.cancel_bubble = TRUE;
				Widget_TriggerEvent( child, &e, NULL );
				child->state = WSTATE_NORMAL;
			}
		}
		ctx.prev = child;
		ctx.prev_display = child->computed_style.display;
	}
	if( w->style->sheet[key_width].type == SVT_AUTO ||
	    w->style->sheet[key_height].type == SVT_AUTO ) {
		Widget_AddTask( w, WTT_RESIZE );
	}
	e.cancel_bubble = TRUE;
	e.type = WET_AFTERLAYOUT;
	Widget_TriggerEvent( w, &e, NULL );
}

static void _LCUIWidget_PrintTree( LCUI_Widget w, int depth, const char *prefix )
{
	int len;
	LCUI_Widget child;
	LinkedListNode *node;
	LCUI_SelectorNode snode;
	char str[16], child_prefix[512];

	len = strlen(prefix);
	strcpy( child_prefix, prefix );
	for( LinkedList_Each( node, &w->children ) ) {
		if( node == w->children.tail.prev ) {
			strcpy( str, "└" );
			strcpy( &child_prefix[len], "    " );
		} else {
			strcpy( str, "├" );
			strcpy( &child_prefix[len], "│  " );
		} 
		strcat( str, "─" );
		child = node->data;
		if( child->children.length == 0 ) {
			strcat( str, "─" );
		} else {
			strcat( str, "┬" );
		}
		snode = Widget_GetSelectorNode( child );
		LOG( "%s%s %s, xy:(%.2f,%.2f), size:(%.2f,%.2f), "
		     "visible: %s, padding: (%.2f,%.2f,%.2f,%.2f), margin: (%.2f,%.2f,%.2f,%.2f)\n",
		     prefix, str, snode->fullname, child->x, child->y,
		     child->width, child->height,
		     child->computed_style.visible ? "true" : "false",
		     child->padding.top, child->padding.right, child->padding.bottom,
		     child->padding.left, child->margin.top, child->margin.right,
		     child->margin.bottom, child->margin.left );
		SelectorNode_Delete( snode );
		_LCUIWidget_PrintTree( child, depth+1, child_prefix );
	}
}

void Widget_PrintTree( LCUI_Widget w )
{
	LCUI_SelectorNode node;
	w = w ? w : LCUIWidget.root;
	node = Widget_GetSelectorNode( w );
	LOG( "%s, xy:(%.2f,%.2f), size:(%.2f,%.2f), visible: %s\n",
	     node->fullname, w->x, w->y, w->width, w->height,
	     w->computed_style.visible ? "true" : "false" );
	SelectorNode_Delete( node );
	_LCUIWidget_PrintTree( w, 0, "  " );
}

static void OnClearWidgetAttribute( void *privdata, void *data )
{
	LCUI_WidgetAttribute attr = data;
	if( attr->value.destructor ) {
		attr->value.destructor( attr->value.data );
	}
	free( attr->name );
	attr->name = NULL;
	attr->value.data = NULL;
}

extern void LCUIWidget_AddTextView( void );
extern void LCUIWidget_AddButton( void );
extern void LCUIWidget_AddSideBar( void );
extern void LCUIWidget_AddTScrollBar( void );
extern void LCUIWidget_AddTextCaret( void );
extern void LCUIWidget_AddTextEdit( void );

void LCUI_InitWidget( void )
{
	LCUIWidget_InitTasks();
	LCUIWidget_InitEvent();
	LCUIWidget_InitPrototype();
	LCUIWidget_InitStyle();
	LCUIWidget_AddTextView();
	LCUIWidget_AddButton();
	LCUIWidget_AddSideBar();
	LCUIWidget_AddTScrollBar();
	LCUIWidget_AddTextCaret();
	LCUIWidget_AddTextEdit();
	LCUIMutex_Init( &LCUIWidget.mutex );
	LCUIWidget.ids = Dict_Create( &DictType_StringKey, NULL );
	LCUIWidget.root = LCUIWidget_New( "root" );
	LCUIWidget.dt_attributes = DictType_StringCopyKey;
	LCUIWidget.dt_attributes.valDestructor = OnClearWidgetAttribute;
	Widget_SetTitleW( LCUIWidget.root, L"LCUI Display" );
}

void LCUI_ExitWidget( void )
{
	LCUIWidget_ExitEvent();
	LCUIWidget_ExitTasks();
	LCUIWidget_ExitPrototype();
}
