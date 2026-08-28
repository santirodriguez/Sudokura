package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"image"
	"image/png"
	"os"
	"path/filepath"
)

var iconSizes = []int{16, 32, 48, 64, 128, 256, 512, 1024}

func must(err error) {
	if err != nil {
		panic(err)
	}
}

func readPNG(path string) image.Image {
	f, err := os.Open(path)
	must(err)
	defer f.Close()
	img, err := png.Decode(f)
	must(err)
	return img
}

func toNRGBA(src image.Image) *image.NRGBA {
	b := src.Bounds()
	dst := image.NewNRGBA(image.Rect(0, 0, b.Dx(), b.Dy()))
	for y := 0; y < b.Dy(); y++ {
		for x := 0; x < b.Dx(); x++ {
			dst.Set(x, y, src.At(b.Min.X+x, b.Min.Y+y))
		}
	}
	return dst
}

func scaled(src image.Image, width, height int) *image.NRGBA {
	b := src.Bounds()
	dst := image.NewNRGBA(image.Rect(0, 0, width, height))
	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			sx := b.Min.X + x*b.Dx()/width
			sy := b.Min.Y + y*b.Dy()/height
			dst.Set(x, y, src.At(sx, sy))
		}
	}
	return dst
}

func pngBytes(img image.Image) []byte {
	var data bytes.Buffer
	encoder := png.Encoder{CompressionLevel: png.BestCompression}
	must(encoder.Encode(&data, img))
	return data.Bytes()
}

func emitRGBA(path, symbol string, img *image.NRGBA) {
	var c bytes.Buffer
	fmt.Fprintf(&c, "#include \"%s.h\"\n", filepath.Base(path))
	fmt.Fprintf(&c, "const unsigned int %s_width=%d,%s_height=%d;\n", symbol, img.Bounds().Dx(), symbol, img.Bounds().Dy())
	fmt.Fprintf(&c, "const unsigned char %s_rgba[]={", symbol)
	for i, v := range img.Pix {
		if i%20 == 0 {
			c.WriteByte('\n')
		}
		fmt.Fprintf(&c, "%d,", v)
	}
	c.WriteString("\n};\n")
	must(os.WriteFile(path+".c", c.Bytes(), 0644))

	header := fmt.Sprintf(
		"#ifndef %s_H\n#define %s_H\nextern const unsigned int %s_width,%s_height;\nextern const unsigned char %s_rgba[];\n#endif\n",
		symbol, symbol, symbol, symbol, symbol,
	)
	must(os.WriteFile(path+".h", []byte(header), 0644))
}

func writeICO(path string, data [][]byte) {
	var out bytes.Buffer
	must(binary.Write(&out, binary.LittleEndian, uint16(0)))
	must(binary.Write(&out, binary.LittleEndian, uint16(1)))
	must(binary.Write(&out, binary.LittleEndian, uint16(6)))
	offset := 6 + 6*16
	for i := 0; i < 6; i++ {
		size := iconSizes[i]
		width, height := byte(size), byte(size)
		if size == 256 {
			width, height = 0, 0
		}
		out.WriteByte(width)
		out.WriteByte(height)
		out.Write([]byte{0, 0})
		must(binary.Write(&out, binary.LittleEndian, uint16(1)))
		must(binary.Write(&out, binary.LittleEndian, uint16(32)))
		must(binary.Write(&out, binary.LittleEndian, uint32(len(data[i]))))
		must(binary.Write(&out, binary.LittleEndian, uint32(offset)))
		offset += len(data[i])
	}
	for i := 0; i < 6; i++ {
		out.Write(data[i])
	}
	must(os.WriteFile(path, out.Bytes(), 0644))
}

func writeICNS(path string, data [][]byte) {
	types := []string{"icp4", "icp5", "icp6", "ic07", "ic08", "ic09", "ic10"}
	indexes := []int{0, 1, 3, 4, 5, 6, 7}
	var body bytes.Buffer
	for i, kind := range types {
		d := data[indexes[i]]
		body.WriteString(kind)
		must(binary.Write(&body, binary.BigEndian, uint32(len(d)+8)))
		body.Write(d)
	}
	var out bytes.Buffer
	out.WriteString("icns")
	must(binary.Write(&out, binary.BigEndian, uint32(body.Len()+8)))
	out.Write(body.Bytes())
	must(os.WriteFile(path, out.Bytes(), 0644))
}

func main() {
	must(os.MkdirAll("assets/generated", 0755))

	iconMaster := readPNG("assets/branding/source/sudokura-icon.png")
	if iconMaster.Bounds().Dx() != 512 || iconMaster.Bounds().Dy() != 512 {
		panic("sudokura-icon.png must be 512x512")
	}

	iconImages := make([]*image.NRGBA, len(iconSizes))
	iconData := make([][]byte, len(iconSizes))
	for i, size := range iconSizes {
		iconImages[i] = scaled(iconMaster, size, size)
		switch size {
		case 16:
			iconData[i], _ = os.ReadFile("assets/branding/source/favicon-16x16.png")
		case 32:
			iconData[i], _ = os.ReadFile("assets/branding/source/favicon-32x32.png")
		case 512:
			iconData[i], _ = os.ReadFile("assets/branding/source/sudokura-icon.png")
		default:
			iconData[i] = pngBytes(iconImages[i])
		}
		if len(iconData[i]) == 0 {
			panic(fmt.Sprintf("missing icon data for %dx%d", size, size))
		}
		must(os.WriteFile(fmt.Sprintf("assets/generated/sudokura-%d.png", size), iconData[i], 0644))
	}

	writeICO("assets/generated/sudokura.ico", iconData)
	writeICNS("assets/generated/sudokura.icns", iconData)
	emitRGBA("assets/generated/window_icon", "sudokura_icon", iconImages[4])

	head := toNRGBA(readPNG("assets/branding/source/sudokura-head.png"))
	if head.Bounds().Dx() != 516 || head.Bounds().Dy() != 166 {
		panic("sudokura-head.png must be 516x166")
	}
	// Keep the historical symbol/file name for this stage so the refreshed
	// branding can replace the existing in-app wordmark without UI refactoring.
	emitRGBA("assets/generated/wordmark", "sudokura_wordmark", head)

	flags := []struct {
		source string
		path   string
		symbol string
	}{
		{"assets/flags/raster/us.png", "assets/generated/flag_us", "sudokura_flag_us"},
		{"assets/flags/raster/ar.png", "assets/generated/flag_ar", "sudokura_flag_ar"},
		{"assets/flags/raster/es-ct.png", "assets/generated/flag_ca", "sudokura_flag_ca"},
	}
	for _, flag := range flags {
		img := toNRGBA(readPNG(flag.source))
		if img.Bounds().Dx() != 96 || img.Bounds().Dy() != 72 {
			panic(flag.source + " must be 96x72")
		}
		emitRGBA(flag.path, flag.symbol, img)
	}

	files, err := filepath.Glob("assets/generated/*")
	must(err)
	fmt.Printf("generated %d branding resources\n", len(files))
}
