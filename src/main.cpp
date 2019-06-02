#include <stdio.h>

#include <sapi/sys.hpp>
#include <sapi/hal.hpp>
#include <sapi/sgfx.hpp>
#include <sapi/var.hpp>

#include "sl_config.h"

static void show_usage(const Cli & cli);
static Point input_to_point(const ConstString & input);
static Area input_to_area(const ConstString & input);
static bool is_memory_ok(const ConstString & application_path, const DisplayDevice & device);

int main(int argc, char * argv[]){
	Cli cli(argc, argv);
	cli.set_publisher(SL_CONFIG_PUBLISHER);
	cli.handle_version();

	String action;
	String device;
	String is_help;
	String is_stdout;
	Point p1, p2, p3, p4;
	Area area;
	String tmp;
	sg_color_t color;
	Printer printer;



	action = cli.get_option("action", "specify the operation line|rect");
	device = cli.get_option("device", "specify the operation line|rect");
	is_help = cli.get_option("help", "display usage");
	is_stdout = cli.get_option("stdout", "print the image to the standard output");

	tmp = cli.get_option("color", "specify color to draw as an integer");
	if( tmp.is_empty() ){
		color = 0xffffffff;
	} else {
		color = tmp.to_integer();
	}

	tmp = cli.get_option("p1", "specify the value of p1 for line|rect|cbez|qbez as x.y");
	p1 = input_to_point(tmp);
	tmp = cli.get_option("p2", "specify the value of p2 for line|cbez|qbez as x.y");
	p2 = input_to_point(tmp);
	tmp = cli.get_option("p3", "specify the value of p3 for line|cbez|qbez as x.y");
	p3 = input_to_point(tmp);
	tmp = cli.get_option("p4", "specify the value of p4 for line|cbez as x.y");
	p4 = input_to_point(tmp);
	tmp = cli.get_option("area", "specify the value of area for rect as wxh");
	area = input_to_area(tmp);

	if( is_help.is_empty() == false ){
		cli.show_options();
		exit(0);
	}

	DisplayDevice display;
	if( device.is_empty() ){
		device = "/dev/display0";
	}

	if( display.open(device, DisplayDevice::READWRITE) < 0 ){
		printer.error("failed to open the display device");
		exit(1);
	}

	if( is_memory_ok(cli.path(), display) == false ){
		printer.error("application does not have enough memory for display");
		exit(1);
	}

	if( display.initialize(device) < 0 ){
		printf("Failed to initialize display (%d, %d)\n",
				 display.return_value(),
				 display.error_number());
	}

	display.ioctl(I_DISPLAY_INIT);


	if( display.to_void() == 0 ){

		DisplayInfo display_info;
		display_info = display.get_info();
		printf("Not enough memory %dx%d %dbpp\n", display_info.width(), display_info.height(), display_info.bits_per_pixel());
		printf("Display needs %d bytes\n", (display.width()*display.height()*display.bits_per_pixel())/8);
		TaskInfo info = TaskManager::get_info();
		printf("Application has %ld bytes\n", info.memory_size());
		exit(1);
	}

	Timer t;

	display.set_pen_color(0);
	display.draw_rectangle(Point(0,0), display.area());
	printer.open_object("display") << display.area() << printer.close();

	display.set_pen_color(color);


	if( action == "line" ){
		printer.open_object("p1") << p1 << printer.close();
		printer.open_object("p2") << p2 << printer.close();
		t.restart();
		display.draw_line(p1, p2);
		t.stop();
	} else if ( action == "rect" ){
		printer.open_object("point") << p1 << printer.close();
		printer.open_object("area") << area << printer.close();
		t.restart();
		display.draw_rectangle(p1, area);
		t.stop();
	} else if ( action == "pixel" ){
		printer.open_object("point") << p1 << printer.close();
		t.restart();
		display.draw_pixel(p1);
		t.stop();
	} else if ( action == "qbez" ){
		printer.open_object("p1") << p1 << printer.close();
		printer.open_object("p2") << p2 << printer.close();
		printer.open_object("p3") << p3 << printer.close();
		t.restart();
		display.draw_quadratic_bezier(p1, p2, p3);
		t.stop();
	} else if ( action == "cbez" ){
		printer.open_object("p1") << p1 << printer.close();
		printer.open_object("p2") << p2 << printer.close();
		printer.open_object("p3") << p3 << printer.close();
		printer.open_object("p4") << p4 << printer.close();
		t.restart();
		display.draw_cubic_bezier(p1, p2, p3, p4);
		t.stop();
	} else if ( action == "palette" ){
		t.restart();
		display.draw_rectangle(Point(0,0), display.area());

		u32 step_width = display.width() / display.bits_per_pixel();
		u32 step_height = display.height() / display.bits_per_pixel();
		u32 count_width = display.bits_per_pixel();
		u32 count_height = display.bits_per_pixel();

		if( display.bits_per_pixel() == 1 ){
			step_height = display.height()/2;
			count_height = 2;
		}

		for(u32 i=0; i < count_width; i++){
			for(u32 j=0; j < count_height; j++){
				display.set_pen_color(i*display.bits_per_pixel() + j);
				display.draw_rectangle(Point(i*step_width, j*step_height), Area(step_width, step_height));
			}
		}
		t.stop();
	} else if ( action == "clear" ){
		t.restart();
		display.clear();
		t.stop();

	} else {
		show_usage(cli);
	}

	printer.key("render time", F32U, t.microseconds());

	printer.key("action", action);
	printer.key("color", "%ld", color);
	printer.key("size", "%ld", ((Bitmap&)display).size());
	printer.key("bmap", "%p", display.bmap());
	printer.key("bpp", "%d", display.bits_per_pixel());

	if( action != "clear" ){
		t.restart();
		display.write(display.bmap(), sizeof(sg_bmap_t));
		t.stop();
		printer.key("write time", F32U, t.microseconds());
	}

	if( is_stdout == "true" ){
		printer << display;
	}

	display.close();

	printer.info("done");

	return 0;
}

bool is_memory_ok(const ConstString & application_path, const DisplayDevice & device){
	bool result = false;

	DisplayInfo display_info;
	AppfsInfo appfs_info;

	display_info = device.get_info();
	if( display_info.is_valid() ){
		appfs_info = Appfs::get_info(application_path);
		if( appfs_info.ram_size() > display_info.memory_size() + 1024 ){
			result = true;
		}
	}
	return result;
}

void show_usage(const Cli & cli){
	printf("%s usage:\n", cli.name().cstring());
	cli.show_options();
	exit(1);
}

Point input_to_point(const ConstString & input){
	Tokenizer values(input, ".");
	return Point(values.at(0).to_integer(), values.at(1).to_integer());
}

Area input_to_area(const ConstString & input){
	Tokenizer values(input, "x");
	return Area(values.at(0).to_integer(), values.at(1).to_integer());
}




