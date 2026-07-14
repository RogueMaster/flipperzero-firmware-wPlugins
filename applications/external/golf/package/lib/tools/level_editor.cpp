#include "../game.hpp"

static constexpr int ZOOM = 14;

static constexpr int WW = 128 * ZOOM;
static constexpr int WH = 64 * ZOOM;

#define IMGUI_IMPLEMENTATION
#include <misc/single_file/imgui_single_file.h>

#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <d3d9.h>
#include <tchar.h>

#include <nfd.h>

#pragma comment(lib, "d3d9.lib")

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cctype>

#define FMT_HEADER_ONLY
#include <fmt/format.h>

struct editor_level_info
{
    std::string name;
    std::vector<int8_t> verts;
    std::vector<uint8_t> faces[5];
    std::vector<phys_box> boxes;
    dvec3 ball_pos;
    dvec3 flag_pos;
    int par;
};

static std::string course_name;
static std::string course_author;
static std::string course_desc;
static std::string course_ident;
static std::vector<editor_level_info> editor_levels;
static int level_index = 0;

static level_info dummy_level;

static bool ortho = false;
static int ortho_zoom = 200;

void save_audio_on_off() {}
void toggle_audio() {}
bool audio_enabled() { return false; }

static ImVec2 dvec2imvec(dvec2 v)
{
    return { float(v.x) / (128 * FB_FRAC_COEF) * WW, float(v.y) / (64 * FB_FRAC_COEF) * WH };
}

static ImVec2 dvec2imvec(dvec3 v)
{
    return dvec2imvec(dvec2{ v.x, v.y });
}

static std::string read_line(FILE* f)
{
    int c;
    std::string r;
    bool valid = false;
    while(!feof(f))
    {
        c = fgetc(f);
        if(c == '>') break;
        if(valid) r += c;
        else if(c == '<') valid = true;
    }
    return r;
}

static void input_coord16(char const* label, int16_t& v)
{
    double fv = double(v) / 256;
    ImGui::SetNextItemWidth(120);
    ImGui::InputDouble(label, &fv, 1.0, 1.0);
    fv = tclamp<double>(fv, -125, 125);
    v = (int16_t)round(fv * 256);
}

static void input_coord(char const* label, int8_t& v)
{
    double fv = double(v) * BOX_POS_FACTOR / 256;
    ImGui::SetNextItemWidth(120);
    ImGui::InputDouble(label, &fv, 1.0, 1.0);
    fv = tclamp<double>(fv, -127.0 * BOX_POS_FACTOR / 256, 127.0 * BOX_POS_FACTOR / 256);
    v = (int8_t)round(fv * (256 / BOX_POS_FACTOR));
}

static void input_size(char const* label, uint8_t& v)
{
    double fv = double(v) * BOX_SIZE_FACTOR / 256;
    ImGui::SetNextItemWidth(120);
    ImGui::InputDouble(label, &fv, 1.0, 1.0);
    fv = tclamp<double>(fv, 0, 255.0 * BOX_SIZE_FACTOR / 256);
    v = (uint8_t)round(fv * (256 / BOX_SIZE_FACTOR));
}

static void input_yaw(char const* label, uint8_t& v)
{
    int iv = v;
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt(label, &iv, 1, 4);
    v = uint8_t(iv);
}

static void input_pitch(char const* label, int8_t& v)
{
    int iv = v;
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt(label, &iv, 1, 4);
    iv = tclamp<int>(iv, -64, 64);
    v = int8_t(iv);
}

static double c2f(int16_t x)
{
    return double(x) * BOX_SIZE_FACTOR / 256;
}

static void ortho_x()
{
    yaw = uint16_t(-64 * 256);
    pitch = 0;
    ortho = true;
    cam = { 30 * 256, 0, 0, };
}

static void ortho_y()
{
    yaw = 0;
    pitch = 0;
    ortho = true;
    cam = { 0, 0, 30 * 256, };
}

static void ortho_z()
{
    yaw = 0;
    pitch = 64 * 256;
    ortho = true;
    cam = { 0, 30 * 256, 0, };
}

static int editor_boxi = 0;

static void editor_load_file(char const* fname)
{
    FILE* f = fopen(fname, "r");
    if(!f) return;
    int r, n0, n1, n2, n3, n4, n5, n6, n7;

    course_ident  = read_line(f);
    course_name   = read_line(f);
    course_author = read_line(f);
    course_desc   = read_line(f);

    r = fscanf(f, "%d\n", &n0);
    if(r != 1) goto error;
    editor_levels.clear();
    editor_levels.resize(n0);

    for(auto& info : editor_levels)
    {
        info.name = read_line(f);

        r = fscanf(f, "%d\n", &info.par);
        if(r != 1) goto error;
        r = fscanf(f, "%d %d %d\n", &n0, &n1, &n2);
        if(r != 3) goto error;
        info.ball_pos.x = n0;
        info.ball_pos.y = n1;
        info.ball_pos.z = n2;
        r = fscanf(f, "%d %d %d\n", &n0, &n1, &n2);
        if(r != 3) goto error;
        info.flag_pos.x = n0;
        info.flag_pos.y = n1;
        info.flag_pos.z = n2;
        {
            auto& boxes = info.boxes;
            r = fscanf(f, "%d\n", &n0);
            if(r != 1) goto error;
            boxes.resize(n0);
            for(auto& b : boxes)
            {
                r = fscanf(f, "%d %d %d %d %d %d %d %d\n",
                    &n0, &n1, &n2, &n3, &n4, &n5, &n6, &n7);
                if(r != 8) goto error;
                b.pos.x  = n0;
                b.pos.y  = n1;
                b.pos.z  = n2;
                b.size.x = n3;
                b.size.y = n4;
                b.size.z = n5;
                b.yaw    = n6;
                b.pitch  = n7;
            }
        }
        r = fscanf(f, "%d\n", &n0);
        if(r != 1) goto error;
        info.verts.resize(n0 * 3);
        for(int j = 0; j < (int)info.verts.size() / 3; ++j)
        {
            r = fscanf(f, "%d %d %d\n", &n0, &n1, &n2);
            if(r != 3) goto error;
            info.verts[j * 3 + 0] = n0;
            info.verts[j * 3 + 1] = n1;
            info.verts[j * 3 + 2] = n2;
        }
        for(int j = 0; j < 5; ++j)
        {
            r = fscanf(f, "%d\n", &n0);
            if(r != 1) goto error;
            info.faces[j].resize(n0 * 3);
            for(int k = 0; k < info.faces[j].size() / 3; ++k)
            {
                r = fscanf(f, "%d %d %d\n", &n0, &n1, &n2);
                if(r != 3) goto error;
                info.faces[j][k * 3 + 0] = n0;
                info.faces[j][k * 3 + 1] = n1;
                info.faces[j][k * 3 + 2] = n2;
            }
        }
    }

    fclose(f);
    return;

error:
    editor_levels.clear();
}

static void editor_save_file(char const* fname)
{
    FILE* f = fopen(fname, "w");
    if(!f) return;

    fprintf(f, "<%s>\n", course_ident.c_str());
    fprintf(f, "<%s>\n", course_name.c_str());
    fprintf(f, "<%s>\n", course_author.c_str());
    fprintf(f, "<%s>\n", course_desc.c_str());

    fprintf(f, "%d\n", (int)editor_levels.size());

    for(auto const& info : editor_levels)
    {
        fprintf(f, "<%s>\n", info.name.c_str());
        fprintf(f, "%d\n", info.par);
        fprintf(f, "%d %d %d\n",
            (int)info.ball_pos.x,
            (int)info.ball_pos.y,
            (int)info.ball_pos.z);
        fprintf(f, "%d %d %d\n",
            (int)info.flag_pos.x,
            (int)info.flag_pos.y,
            (int)info.flag_pos.z);
        {
            auto const& boxes = info.boxes;
            fprintf(f, "%d\n", (int)boxes.size());
            for(auto b : boxes)
            {
                fprintf(f, "%d %d %d %d %d %d %d %d\n",
                    (int)b.pos.x, (int)b.pos.y, (int)b.pos.z,
                    (int)b.size.x, (int)b.size.y, (int)b.size.z,
                    (int)b.yaw, (int)b.pitch);
            }
        }
        fprintf(f, "%d\n", (int)info.verts.size() / 3);
        for(int j = 0; j < info.verts.size() / 3; ++j)
        {
            fprintf(f, "%d %d %d\n",
                (int)info.verts[j * 3 + 0],
                (int)info.verts[j * 3 + 1],
                (int)info.verts[j * 3 + 2]);
        }
        for(int j = 0; j < 5; ++j)
        {
            fprintf(f, "%d\n", (int)info.faces[j].size() / 3);
            for(int k = 0; k < info.faces[j].size() / 3; ++k)
            {
                fprintf(f, "%d %d %d\n",
                    (int)info.faces[j][k * 3 + 0],
                    (int)info.faces[j][k * 3 + 1],
                    (int)info.faces[j][k * 3 + 2]);
            }
        }
    }
  
    fclose(f);
}

static void editor_save_header(char const* fname)
{
    FILE* f = fopen(fname, "w");
    if(!f) return;

    fprintf(f, "#pragma once\n\n");
    fprintf(f, "// Generated file: do not edit.\n\n");
    fprintf(f, "// This file is used only for the non-FX version of ArduGolf.\n\n");

    for(int i = 0; i < (int)editor_levels.size(); ++i)
    {
        auto const& info = editor_levels[i];

        fprintf(f, "static int8_t const LEVELS_%s_%02d_VERTS[%d * 3] PROGMEM =\n{\n",
            course_ident.c_str(), i, (int)info.verts.size() / 3);
        for(int j = 0; j < (int)info.verts.size() / 3; ++j)
        {
            fprintf(f, "    %d, %d, %d,\n",
                (int)info.verts[j * 3 + 0],
                (int)info.verts[j * 3 + 1],
                (int)info.verts[j * 3 + 2]);
        }
        fprintf(f, "};\n\n");

        {
            int num_faces = 0;
            for(int k = 0; k < 5; ++k)
                num_faces += (int)info.faces[k].size() / 3;
            fprintf(f, "static uint8_t const LEVELS_%s_%02d_FACES[%d * 3] PROGMEM =\n{\n",
                course_ident.c_str(), i, num_faces);
        }
        for(int k = 0; k < 5; ++k)
        {
            for(int j = 0; j < (int)info.faces[k].size() / 3; ++j)
            {
                fprintf(f, "    %d, %d, %d,\n",
                    (int)info.faces[k][j * 3 + 0],
                    (int)info.faces[k][j * 3 + 1],
                    (int)info.faces[k][j * 3 + 2]);
            }
        }
        fprintf(f, "};\n\n");

        fprintf(f, "static phys_box const LEVELS_%s_%02d_BOXES[%d] PROGMEM =\n{\n",
            course_ident.c_str(), i, (int)info.boxes.size());
        for(auto const& b : info.boxes)
        {
            fprintf(f, "    { { %d, %d, %d }, { %d, %d, %d }, %d, %d },\n",
                (int)b.size.x, (int)b.size.y, (int)b.size.z,
                (int)b.pos.x, (int)b.pos.y, (int)b.pos.z,
                (int)b.yaw, (int)b.pitch);
        }
        fprintf(f, "};\n\n");
    }

    fprintf(f, "static level_info const LEVELS_%s[%d] PROGMEM =\n{\n\n",
        course_ident.c_str(), (int)editor_levels.size());

    for(int i = 0; i < (int)editor_levels.size(); ++i)
    {
        auto const& info = editor_levels[i];

        fprintf(f, "{\n");
        fprintf(f, "    LEVELS_%s_%02d_VERTS,\n", course_ident.c_str(), i);
        fprintf(f, "    LEVELS_%s_%02d_FACES,\n", course_ident.c_str(), i);
        fprintf(f, "    LEVELS_%s_%02d_BOXES,\n", course_ident.c_str(), i);
        fprintf(f, "    {\n");
        fprintf(f, "        %d,\n", (int)info.verts.size() / 3);
        fprintf(f, "        { %d, %d, %d, %d, %d, },\n",
            (int)info.faces[0].size() / 3,
            (int)info.faces[1].size() / 3,
            (int)info.faces[2].size() / 3,
            (int)info.faces[3].size() / 3,
            (int)info.faces[4].size() / 3);
        fprintf(f, "        %d,\n",
            (int)info.faces[0].size() / 3 +
            (int)info.faces[1].size() / 3 +
            (int)info.faces[2].size() / 3 +
            (int)info.faces[3].size() / 3 +
            (int)info.faces[4].size() / 3);
        fprintf(f, "        %d,\n", (int)info.boxes.size());
        fprintf(f, "        { %d, %d, %d },\n",
            (int)info.ball_pos.x,
            (int)info.ball_pos.y,
            (int)info.ball_pos.z);
        fprintf(f, "        { %d, %d, %d },\n",
            (int)info.flag_pos.x,
            (int)info.flag_pos.y,
            (int)info.flag_pos.z);
        fprintf(f, "        %d,\n", info.par);
        fprintf(f, "    },\n},\n\n");
    }

    fprintf(f, "};\n");

    fclose(f);
}

static void editor_save_fxbin(char const* fname)
{
    FILE* f = fopen(fname, "wb");
    if(!f) return;

    // level header page
    {
        struct
        {
            fx_level_header header;
            uint8_t padding[256 - sizeof(fx_level_header)];
        } header_padded{};
        auto& header = header_padded.header;
        header.num_holes = (uint8_t)editor_levels.size();
        for(size_t i = 0; i < 18; ++i)
            if(i < editor_levels.size())
                header.pars[i] = editor_levels[i].par;
        memcpy(
            &header.name[0],
            course_name.c_str(),
            course_name.size());
        memcpy(
            &header.author[0],
            course_author.c_str(),
            course_author.size());
        memcpy(
            &header.desc[0],
            course_desc.c_str(),
            course_desc.size());
        fwrite(&header_padded, sizeof(header_padded), 1, f);
    }

    for(auto const& info : editor_levels)
    {
        struct
        {
            fx_level_info fxinfo{};
            uint8_t padding[1024 - sizeof(fx_level_info)];
        } fxinfo_padded{};
        auto& fxinfo = fxinfo_padded.fxinfo;
        fxinfo.ext.num_verts = uint8_t(info.verts.size() / 3);
        for(int i = 0; i < info.verts.size() / 3; ++i)
        {
            fxinfo.vxz[i * 2 + 0] = info.verts[i * 3 + 0];
            fxinfo.vy [i        ] = info.verts[i * 3 + 1];
            fxinfo.vxz[i * 2 + 1] = info.verts[i * 3 + 2];
        }
        for(int i = 0; i < 5; ++i)
        {
            for(int j = 0; j < (int)info.faces[i].size(); ++j)
                fxinfo.faces[fxinfo.ext.num_faces * 3 + j] = info.faces[i][j];
            fxinfo.ext.pat_faces[i] = (int)info.faces[i].size() / 3;
            fxinfo.ext.num_faces += fxinfo.ext.pat_faces[i];
        }
        fxinfo.ext.num_boxes = (uint8_t)info.boxes.size();
        for(int i = 0; i < (int)info.boxes.size(); ++i)
            fxinfo.boxes[i] = info.boxes[i];
        fxinfo.ext.ball_pos = info.ball_pos;
        fxinfo.ext.flag_pos = info.flag_pos;
        fxinfo.ext.par = (uint8_t)info.par;
        fwrite(&fxinfo_padded, sizeof(fxinfo_padded), 1, f);
    }

    fclose(f);
}

static int get_pattern_of_material(char const* mat_name)
{
    for(;;)
    {
        char c = *mat_name++;
        if(c == '\0') break;
        if(c >= '0' && c <= '4')
            return c - '0';
    }
    return 0;
}

static void load_mesh(char const* fname)
{
    std::ifstream f(fname);
    if(f.fail()) return;
    std::string s;

    auto& info = editor_levels[level_index];
    info.verts.clear();
    for(auto& f : info.faces) f.clear();
    int pat = 0;

    while(!f.eof())
    {
        std::getline(f, s);
        std::stringstream ss(s);
        std::string word;
        std::getline(ss, word, ' ');
        if(word == "v")
        {
            double x, y, z;
            ss >> x >> y >> z;
            info.verts.push_back(int8_t(round(x * 4)));
            info.verts.push_back(int8_t(round(y * 4)));
            info.verts.push_back(int8_t(round(z * 4)));
        }
        else if(word == "f")
        {
            char dummy;
            int i0, i1, i2;
            ss >> i0;
            while(ss.peek() > ' ') ss >> dummy;
            ss >> i1;
            while(ss.peek() > ' ') ss >> dummy;
            ss >> i2;
            while(ss.peek() > ' ') ss >> dummy;
            info.faces[pat].push_back(uint8_t(i0 - 1));
            info.faces[pat].push_back(uint8_t(i1 - 1));
            info.faces[pat].push_back(uint8_t(i2 - 1));
        }
        else if(word == "usemtl")
        {
            pat = 0;
            for(char c : s)
            {
                if(c >= '0' && c <= '4')
                {
                    pat = c - '0';
                    break;
                } 
            }
        }
    }
}

static int total_faces(editor_level_info const& info)
{
    int r = 0;
    for(int i = 0; i < 5; ++i)
        r += (int)info.faces[i].size() / 3;
    return r;
}

static void editor_gui()
{
    using namespace ImGui;
    SetNextWindowSize({ 500, 600 }, ImGuiCond_FirstUseEver);
    Begin("Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if(Button("New Course"))
    {
        editor_levels.clear();
        editor_levels.push_back({});
    }
    SameLine();
    if(Button("Load from file"))
    {
        nfdchar_t* path;
        auto r = NFD_OpenDialog("txt", BASE_DIR "\\levels", &path);
        if(r == NFD_ERROR)
            r = NFD_OpenDialog("txt", nullptr, &path);
        if(r == NFD_OKAY)
        {
            editor_load_file(path);
            free(path);
        }
    }
    if(editor_levels.empty())
    {
        End();
        return;
    } 
    SameLine();
    if(Button("Save to file"))
    {
        nfdchar_t* path;
        auto r = NFD_SaveDialog("txt", BASE_DIR "\\levels", &path);
        if(r == NFD_ERROR)
            r = NFD_SaveDialog("txt", nullptr, &path);
        if(r == NFD_OKAY)
        {
            editor_save_file(path);
            free(path);
        }
    }
    SameLine();
    if(Button("Save to header"))
    {
        nfdchar_t* path;
        auto r = NFD_SaveDialog("hpp", BASE_DIR "\\levels", &path);
        if(r == NFD_ERROR)
            r = NFD_SaveDialog("hpp", nullptr, &path);
        if(r == NFD_OKAY)
        {
            editor_save_header(path);
            free(path);
        }
    }
    SameLine();
    if(Button("Save to FX bin"))
    {
        nfdchar_t* path;
        auto r = NFD_SaveDialog("bin", BASE_DIR "\\levels", &path);
        if(r == NFD_ERROR)
            r = NFD_SaveDialog("bin", nullptr, &path);
        if(r == NFD_OKAY)
        {
            editor_save_fxbin(path);
            free(path);
        }
    }

    Checkbox("Ortho", &ortho);
    SameLine();
    InputInt("Ortho Zoom", &ortho_zoom, 50);
    ortho_zoom = tmax(ortho_zoom, 50);
    if(Button("Ortho X")) ortho_x();
    SameLine();
    if(Button("Ortho Y")) ortho_y();
    SameLine();
    if(Button("Ortho Z")) ortho_z();

    {
        char buf[sizeof(fx_level_header::name)];
        strncpy(buf, course_name.c_str(), sizeof(buf));
        InputText("Course Name", buf, sizeof(buf));
        course_name = buf;
    }
    {
        char buf[sizeof(fx_level_header::author)];
        strncpy(buf, course_author.c_str(), sizeof(buf));
        InputText("Course Author", buf, sizeof(buf));
        course_author = buf;
    }
    {
        char buf[sizeof(fx_level_header::desc)];
        strncpy(buf, course_desc.c_str(), sizeof(buf));
        InputText("Course Description", buf, sizeof(buf));
        course_desc = buf;
    }
    {
        char buf[64];
        strncpy(buf, course_ident.c_str(), sizeof(buf));
        InputText("Course Identifier", buf, sizeof(buf));
        course_ident = buf;
        for(auto& c : course_ident)
            if(!isalnum(c)) c = '_';
    }

    std::vector<std::string> levelstrs;
    std::vector<char const*> levelstrs2;
    for(int i = 0; i < (int)editor_levels.size(); ++i)
    {
        auto const& info = editor_levels[i];
        levelstrs.push_back(fmt::format(
            "Level {:02} [{:2}v {:2}f {:2}b] {}", i,
            (int)info.verts.size() / 3,
            total_faces(info),
            (int)info.boxes.size(),
            info.name));
        levelstrs2.push_back(levelstrs.back().c_str());
    }
    SetNextItemWidth(-1);
    ListBox("##Levels", &level_index, levelstrs2.data(), (int)levelstrs2.size());
    //InputInt("Level Index", &level_index);

    level_index = tclamp(level_index, 0, (int)editor_levels.size() - 1);

    if(Button("Add Level"))
    {
        editor_levels.insert(editor_levels.begin() + (level_index + 1), 1, {});
        ++level_index;
    }
    SameLine();
    if(Button("Remove Level"))
    {
        editor_levels.erase(editor_levels.begin() + level_index);
        level_index = tclamp(level_index, 0, (int)editor_levels.size() - 1);
        End();
        return;
    }
    SameLine();
    if(Button("Move Level Up") && level_index > 0)
    {
        std::swap(editor_levels[level_index - 1], editor_levels[level_index]);
        --level_index;
    }
    SameLine();
    if(Button("Move Level Down") && level_index < (int)editor_levels.size() - 1)
    {
        std::swap(editor_levels[level_index + 1], editor_levels[level_index]);
        ++level_index;
    }

    auto& level = editor_levels[level_index];
    int num_boxes = (int)level.boxes.size();

    {
        char buf[256];
        strncpy(buf, level.name.c_str(), sizeof(buf));
        InputText("Name", buf, sizeof(buf));
        level.name = buf;
    }

    if(Button("Load mesh from file"))
    {
        nfdchar_t* path;
        char const* filters = "obj";
        auto r = NFD_OpenDialog(filters, BASE_DIR "\\levels", &path);
        if(r == NFD_ERROR)
            r = NFD_OpenDialog(filters, nullptr, &path);
        if(r == NFD_OKAY)
        {
            load_mesh(path);
            free(path);
        }
    }

    dummy_level.ext.ball_pos = level.ball_pos;
    dummy_level.ext.flag_pos = level.flag_pos;

    InputInt("Par", &level.par);
    level.par = tclamp(level.par, 1, 8);

    AlignTextToFramePadding(); Text("Ball:"); SameLine();
    input_coord16("x##ballx", level.ball_pos.x); SameLine();
    input_coord16("y##bally", level.ball_pos.y); SameLine();
    input_coord16("z##ballz", level.ball_pos.z);
    AlignTextToFramePadding(); Text("Flag:"); SameLine();
    input_coord16("x##flagx", level.flag_pos.x); SameLine();
    input_coord16("y##flagy", level.flag_pos.y); SameLine();
    input_coord16("z##flagz", level.flag_pos.z);

    std::vector<std::string> boxstrs;
    std::vector<char const*> boxstrs2;
    for(int i = 0; i < num_boxes; ++i)
    {
        phys_box const& box = level.boxes[i];
        boxstrs.push_back(fmt::format(
            "Box {:02d}: {:4.1f} {:4.1f} {:4.1f}   {}/{}",
            i,
            c2f(box.size.x), c2f(box.size.y), c2f(box.size.z),
            box.yaw, box.pitch));
    }
    for(size_t i = 0; i < boxstrs.size(); ++i)
        boxstrs2.push_back(boxstrs[i].c_str());
    auto& boxes = level.boxes;
    if(Button("Clone Box"))
    {
        boxes.push_back(boxes[editor_boxi]);
        editor_boxi = (int)boxes.size() - 1;
    }
    SameLine();
    if(Button("Add Box"))
    {
        boxes.push_back({});
        editor_boxi = (int)boxes.size() - 1;
    }
    SameLine();
    if(Button("Remove Box") && boxes.size() > 1)
    {
        boxes.erase(boxes.begin() + editor_boxi);
        editor_boxi = tclamp(editor_boxi, 0, (int)boxes.size() - 1);
    }
    SetNextItemWidth(-1);
    ListBox("##Boxes", &editor_boxi, boxstrs2.data(), (int)boxstrs2.size());
    if(editor_boxi >= 0 && editor_boxi < num_boxes)
    {
        phys_box& box = boxes[editor_boxi];
        AlignTextToFramePadding(); Text("Pos:"); SameLine(); SetCursorPosX(65);
        input_coord("x##posx", box.pos.x); SameLine();
        input_coord("y##posy", box.pos.y); SameLine();
        input_coord("z##posz", box.pos.z);
        AlignTextToFramePadding(); Text("Size:"); SameLine(); SetCursorPosX(65);
        input_size("x##sizex", box.size.x); SameLine();
        input_size("y##sizey", box.size.y); SameLine();
        input_size("z##sizez", box.size.z);
        AlignTextToFramePadding(); Text("Angles:"); SameLine(); SetCursorPosX(65);
        input_yaw("y##boxyaw", box.yaw); SameLine();
        input_pitch("p##boxpitch", box.pitch);
    }
    End();
}

static void editor_draw_box(phys_box box, ImU32 col, float thickness)
{
    static int const CORNERS[8 * 3] =
    {
        -1, -1, -1,
        -1, -1, +1,
        -1, +1, -1,
        -1, +1, +1,
        +1, -1, -1,
        +1, -1, +1,
        +1, +1, -1,
        +1, +1, +1,
    };
    auto* draw = ImGui::GetBackgroundDrawList();

    dvec3 p[8];
    uvec3 size = box.size;
    mat3 m;
    rotation_phys(m, box.yaw, box.pitch);
    for(int j = 0; j < 8; ++j)
    {
        p[j].x = box.pos.x * BOX_POS_FACTOR;
        p[j].y = box.pos.y * BOX_POS_FACTOR;
        p[j].z = box.pos.z * BOX_POS_FACTOR;
        dvec3 t;
        t.x = box.size.x * BOX_SIZE_FACTOR;
        t.y = box.size.y * BOX_SIZE_FACTOR;
        t.z = box.size.z * BOX_SIZE_FACTOR;
        t.x *= CORNERS[j * 3 + 0];
        t.y *= CORNERS[j * 3 + 1];
        t.z *= CORNERS[j * 3 + 2];
        if(box.yaw != 0 || box.pitch != 0)
            t = matvec(m, t);
        p[j].x += t.x;
        p[j].y += t.y;
        p[j].z += t.z;
        p[j] = transform_point(p[j], ortho, ortho_zoom);
        if(p[j].z < 128) return;
    }

    draw->AddLine(dvec2imvec(p[0]), dvec2imvec(p[1]), col, thickness);
    draw->AddLine(dvec2imvec(p[0]), dvec2imvec(p[2]), col, thickness);
    draw->AddLine(dvec2imvec(p[1]), dvec2imvec(p[3]), col, thickness);
    draw->AddLine(dvec2imvec(p[2]), dvec2imvec(p[3]), col, thickness);

    draw->AddLine(dvec2imvec(p[4]), dvec2imvec(p[5]), col, thickness);
    draw->AddLine(dvec2imvec(p[4]), dvec2imvec(p[6]), col, thickness);
    draw->AddLine(dvec2imvec(p[5]), dvec2imvec(p[7]), col, thickness);
    draw->AddLine(dvec2imvec(p[6]), dvec2imvec(p[7]), col, thickness);

    draw->AddLine(dvec2imvec(p[0]), dvec2imvec(p[4]), col, thickness);
    draw->AddLine(dvec2imvec(p[1]), dvec2imvec(p[5]), col, thickness);
    draw->AddLine(dvec2imvec(p[2]), dvec2imvec(p[6]), col, thickness);
    draw->AddLine(dvec2imvec(p[3]), dvec2imvec(p[7]), col, thickness);
}

static void editor_draw_boxes()
{
    auto const& boxes = editor_levels[level_index].boxes;
    for(int j, i = 0; i < (int)boxes.size(); ++i)
    {
        auto const& box = boxes[i];

        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 1, 1));
        float thickness = 1.f;
        if(i == editor_boxi)
        {
            col = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 0, 0, 1));
            thickness = 3.f;
        }

        editor_draw_box(box, col, thickness);
    }
}

uint8_t poll_btns() { return 0; }

// Data
static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("ArduGolf Editor"), WS_OVERLAPPEDWINDOW, 100, 100, WW, WH, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if(!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    current_level = &dummy_level;
    yaw = 0;
    pitch = 4000;
    cam = { 0, 12 * 256, 32 * 256 };
    ball = { 256 * 16, 256 * 2, 256 * 1 };

    // Main loop
    bool done = false;
    while(!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        MSG msg;
        while(::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if(msg.message == WM_QUIT)
                done = true;
        }
        if(done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        int look_speed = 128;
        int move_speed = 64;

        if(ImGui::IsKeyDown(ImGuiKey_A))
        {
            if(ImGui::IsKeyDown(ImGuiKey_UpArrow   )) look_up(look_speed);
            if(ImGui::IsKeyDown(ImGuiKey_DownArrow )) look_up(-look_speed);
            if(ImGui::IsKeyDown(ImGuiKey_LeftArrow )) look_right(-look_speed);
            if(ImGui::IsKeyDown(ImGuiKey_RightArrow)) look_right(look_speed);
        }
        else if(ImGui::IsKeyDown(ImGuiKey_B))
        {
            if(ImGui::IsKeyDown(ImGuiKey_UpArrow  )) move_up(move_speed);
            if(ImGui::IsKeyDown(ImGuiKey_DownArrow)) move_up(-move_speed);
        }
        else
        {
            if(ImGui::IsKeyDown(ImGuiKey_UpArrow   )) move_forward(move_speed);
            if(ImGui::IsKeyDown(ImGuiKey_DownArrow )) move_forward(-move_speed);
            if(ImGui::IsKeyDown(ImGuiKey_LeftArrow )) move_right(-move_speed);
            if(ImGui::IsKeyDown(ImGuiKey_RightArrow)) move_right(move_speed);

            if(ImGui::IsKeyDown(ImGuiKey_Keypad1)) ortho_x();
            if(ImGui::IsKeyDown(ImGuiKey_Keypad3)) ortho_z();
            if(ImGui::IsKeyDown(ImGuiKey_Keypad7)) ortho_y();
        }

        //ImGui::ShowDemoWindow(nullptr);

        editor_gui();

        if(!editor_levels.empty())
        {
            auto const& info = editor_levels[level_index];
            std::vector<uint8_t> faces;
            uint8_t pat_faces[5];
            for(int i = 0; i < 5; ++i)
            {
                pat_faces[i] = info.faces[i].size() / 3;
                faces.insert(faces.end(), info.faces[i].begin(), info.faces[i].end());
            }
            uint8_t nfpat[4];
            nfpat[0] = pat_faces[0];
            nfpat[1] = nfpat[0] + pat_faces[1];
            nfpat[2] = nfpat[1] + pat_faces[2];
            nfpat[3] = nfpat[2] + pat_faces[3];
            uint8_t nf;
            dummy_level.verts = info.verts.data();
            dummy_level.faces = faces.data();
            dummy_level.ext.num_verts = uint8_t(info.verts.size() / 3);
            dummy_level.ext.num_faces = 0;
            for(int i = 0; i < 5; ++i)
            {
                dummy_level.ext.pat_faces[i] = pat_faces[i];
                dummy_level.ext.num_faces += pat_faces[i];
            }
            load_level_from_prog();
            if(ortho)
                nf = render_scene_ortho(ortho_zoom);
            else
                nf = render_scene_persp();
            auto* draw = ImGui::GetBackgroundDrawList();
            for(uint8_t i = 0; i < nf; ++i)
            {
                auto f = fs[i];
                if(f.pt == 255) continue;
                static ImU32 const COLORS[5] =
                {
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.00f, 0.00f, 0.00f, 1.f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.25f, 0.25f, 0.25f, 1.f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.50f, 0.50f, 0.50f, 1.f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.75f, 0.75f, 0.75f, 1.f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(1.00f, 1.00f, 1.00f, 1.f)),
                };
                draw->AddTriangleFilled(
                    dvec2imvec(vs[f.i0]),
                    dvec2imvec(vs[f.i1]),
                    dvec2imvec(vs[f.i2]),
                    COLORS[f.pt]);
            }

            editor_draw_boxes();

            {
                auto ball = info.ball_pos;
                uint8_t r = BALL_RADIUS / BOX_SIZE_FACTOR;
                vec3 bp;
                bp.x = ball.x / BOX_POS_FACTOR;
                bp.y = ball.y / BOX_POS_FACTOR;
                bp.z = ball.z / BOX_POS_FACTOR;
                editor_draw_box(
                    { { r, r, r }, bp },
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.f, 1.f, 0.5f, 1.f)),
                    3.f);
            }
            {
                auto flag = info.flag_pos;
                uint8_t r = FLAG_RADIUS / BOX_SIZE_FACTOR;
                vec3 fp;
                fp.x = flag.x / BOX_POS_FACTOR;
                fp.y = flag.y / BOX_POS_FACTOR;
                fp.z = flag.z / BOX_POS_FACTOR;
                editor_draw_box(
                    { { r, r, r }, fp },
                    ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 0.f, 1.f, 1.f)),
                    3.f);
            }
        }

#if 0
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if(show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if(ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if(show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if(ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
#endif

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if(g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

        // Handle loss of D3D9 device
        if(result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if(g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    if(g_pD3D) { g_pD3D->Release(); g_pD3D = NULL; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if(hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if(ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch(msg)
    {
    case WM_SIZE:
        if(g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
