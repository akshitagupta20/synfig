/* === S Y N F I G ========================================================= */
/*!	\file dock_toolbox.cpp
**	\brief writeme
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
**  Copyright (c) 2008 Paul Wise
**	Copyright (c) 2009 Nikita Kitaev
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
**
** === N O T E S ===========================================================
**
** ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <gtk/gtk.h>
#include <gtkmm/accelmap.h>

#include <sigc++/signal.h>
#include <sigc++/hide.h>
#include <sigc++/slot.h>
#include <sigc++/retype_return.h>
#include <sigc++/retype.h>

#include "general.h"
#include <sstream>

#include "docks/dock_toolbox.h"
#include "instance.h"
#include "app.h"
#include "canvasview.h"
#include "dialogs/dialog_gradient.h"
#include "dialogs/dialog_color.h"
#include "docks/dialog_tooloptions.h"
#include "docks/dockable.h"
#include "docks/dockmanager.h"
#include "docks/dockdialog.h"
#include "widgets/widget_defaults.h"
#include <synfigapp/main.h>

#endif

using namespace std;
using namespace etl;
using namespace synfig;
using namespace studio;
using namespace sigc;

/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */


Dock_Toolbox::Dock_Toolbox():
	Dockable("toolbox",_("Toolbox"),Gtk::StockID("synfig-toolbox"))
{
	set_use_scrolled(false);
	set_size_request(-1,-1);

	tool_alignment = manage(new class Gtk::Alignment(Gtk::ALIGN_CENTER, Gtk::ALIGN_TOP, 0.0, 0.0));	
	tool_table = manage(new class Gtk::Table());
	tool_alignment->add(*tool_table);

	separator = manage(new class Gtk::HSeparator());

	default_widgets_alignment = manage(new class Gtk::Alignment(Gtk::ALIGN_CENTER, Gtk::ALIGN_TOP, 0.0, 0.0));		
	Widget_Defaults* widget_defaults(manage(new Widget_Defaults()));
	default_widgets_alignment->add(*widget_defaults);

	// pack tools and default widgets
	tool_box = manage(new class Gtk::VBox(false, 2));
	tool_box->pack_start(*tool_alignment, Gtk::PACK_SHRINK, 0);
	tool_box->pack_start(*separator, Gtk::PACK_SHRINK, 0);
	tool_box->pack_start(*default_widgets_alignment, Gtk::PACK_SHRINK, 0);
	tool_box->set_border_width(2);
	tool_box->show_all();

	add(*tool_box);

	App::signal_instance_selected().connect(
		sigc::hide(
			sigc::mem_fun(*this,&studio::Dock_Toolbox::update_tools)
		)
	);

	std::list<Gtk::TargetEntry> listTargets;
	listTargets.push_back( Gtk::TargetEntry("text/plain") );
	listTargets.push_back( Gtk::TargetEntry("image") );
//	listTargets.push_back( Gtk::TargetEntry("image/x-sif") );

	drag_dest_set(listTargets);
	signal_drag_data_received().connect( sigc::mem_fun(*this, &studio::Dock_Toolbox::on_drop_drag_data_received) );

	changing_state_=false;

	App::signal_present_all().connect(sigc::mem_fun0(*this,&Dock_Toolbox::present));
}

Dock_Toolbox::~Dock_Toolbox()
{
	hide();
	//studio::App::cb.task(_("Toolbox: I was nailed!"));
	//studio::App::quit();

	if(studio::App::dock_toolbox==this)
		studio::App::dock_toolbox=NULL;
}

void
Dock_Toolbox::set_active_state(const synfig::String& statename)
{
	std::map<synfig::String,Gtk::ToggleButton *>::iterator iter;

	changing_state_=true;

	synfigapp::Main::set_state(statename);

	try
	{

		for(iter=state_button_map.begin();iter!=state_button_map.end();++iter)
		{
			if(iter->first==statename)
			{
				if(!iter->second->get_active())
					iter->second->set_active(true);
			}
			else
			{
				if(iter->second->get_active())
					iter->second->set_active(false);
			}
		}
	}
	catch(...)
	{
		changing_state_=false;
		throw;
	}
	changing_state_=false;
}

void
Dock_Toolbox::change_state(const synfig::String& statename, bool force)
{
	etl::handle<studio::CanvasView> canvas_view(studio::App::get_selected_canvas_view());
	if(canvas_view)
	{
		if(!force && statename==canvas_view->get_smach().get_state_name())
		{
			return;
		}

		if(state_button_map.count(statename))
		{
			state_button_map[statename]->clicked();
		}
		else
		{
			synfig::error("Unknown state \"%s\"",statename.c_str());
		}
	}
}

void
Dock_Toolbox::change_state_(const Smach::state_base *state)
{
	if(changing_state_)
		return;
	changing_state_=true;

	try
	{
		etl::handle<studio::CanvasView> canvas_view(studio::App::get_selected_canvas_view());
		if(canvas_view)
				canvas_view->get_smach().enter(state);
		else
			refresh();
	}
	catch(...)
	{
		changing_state_=false;
		throw;
	}

	changing_state_=false;
}


/*! \fn Dock_Toolbox::add_state(const Smach::state_base *state)
 *  \brief Add and connect a toogle button to the toolbox defined by a state
 *  \param state a const pointer to Smach::state_base
*/
void
Dock_Toolbox::add_state(const Smach::state_base *state)
{
	Gtk::Image *icon;

	assert(state);

	String name=state->get_name();

	Gtk::StockItem stock_item;
	Gtk::Stock::lookup(Gtk::StockID("synfig-"+name),stock_item);

	tool_button=manage(new class Gtk::ToggleButton());

	Gtk::AccelKey key;
	//Have a look to global fonction init_ui_manager() from app.cpp for "accel_path" definition
	Gtk::AccelMap::lookup_entry ("<Actions>/action_group_state_manager/state-"+name, key);
	//Gets the accelerator representation for labels
	Glib::ustring accel_path = key.get_abbrev ();

	Gtk::IconSize iconsize = Gtk::IconSize::from_name("synfig-small_icon_16x16");
	icon=manage(new Gtk::Image(stock_item.get_stock_id(), iconsize));
	tool_button->add(*icon);
	tool_button->set_tooltip_text(stock_item.get_label()+" "+accel_path);
	tool_button->set_relief(Gtk::RELIEF_NONE);
	icon->show();
	tool_button->show();

	int row=state_button_map.size()/5;
	int col=state_button_map.size()%5;

	tool_table->attach(*tool_button,col,col+1,row,row+1, Gtk::FILL, Gtk::FILL, 0, 0);

	state_button_map[name]=tool_button;

	tool_button->signal_clicked().connect(
		sigc::bind(
			sigc::mem_fun(*this,&studio::Dock_Toolbox::change_state_),
			state
		)
	);

	refresh();
}


void
Dock_Toolbox::update_tools()
{
	etl::handle<Instance> instance=App::get_selected_instance();
	etl::handle<CanvasView> canvas_view=App::get_selected_canvas_view();

	// These next several lines just adjust the tool buttons
	// so that they are only clickable when they should be.
	if(instance && canvas_view)
	{
		std::map<synfig::String,Gtk::ToggleButton *>::iterator iter;

		for(iter=state_button_map.begin();iter!=state_button_map.end();++iter)
			iter->second->set_sensitive(true);
	}
	else
	{
		std::map<synfig::String,Gtk::ToggleButton *>::iterator iter;

		for(iter=state_button_map.begin();iter!=state_button_map.end();++iter)
			iter->second->set_sensitive(false);
	}

	if(canvas_view && canvas_view->get_smach().get_state_name())
	{
		set_active_state(canvas_view->get_smach().get_state_name());
	}
	else
		set_active_state("none");

}

void
Dock_Toolbox::on_drop_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int /*x*/, int /*y*/, const Gtk::SelectionData& selection_data_, guint /*info*/, guint time)
{
	// We will make this true once we have a solid drop
	bool success(false);

	if ((selection_data_.get_length() >= 0) && (selection_data_.get_format() == 8))
	{
		synfig::String selection_data((gchar *)(selection_data_.get_data()));

		// For some reason, GTK hands us a list of URLs separated
		// by not only Carriage-Returns, but also Line-Feeds.
		// Line-Feeds will mess us up. Remove all the line-feeds.
		while(selection_data.find_first_of('\r')!=synfig::String::npos)
			selection_data.erase(selection_data.begin()+selection_data.find_first_of('\r'));

		std::stringstream stream(selection_data);

		while(stream)
		{
			synfig::String filename,URI;
			getline(stream,filename);

			// If we don't have a filename, move on.
			if(filename.empty())
				continue;

			// Make sure this URL is of the "file://" type.
			URI=String(filename.begin(),filename.begin()+sizeof("file://")-1);
			if(URI!="file://")
			{
				synfig::warning("Unknown URI (%s) in \"%s\"",URI.c_str(),filename.c_str());
				continue;
			}

			// Strip the "file://" part from the filename
			filename=synfig::String(filename.begin()+sizeof("file://")-1,filename.end());

			synfig::info("Attempting to open "+filename);
			if(App::open(filename))
				success=true;
			else
				synfig::error("Drop failed: Unable to open "+filename);
		}
	}
	else
		synfig::error("Drop failed: bad selection data");

	// Finish the drag
	context->drag_finish(success, false, time);
}
