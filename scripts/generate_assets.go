package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"image"
	"image/color"
	"image/draw"
	"image/png"
	"os"
	"path/filepath"
)

var sizes = []int{16, 32, 48, 64, 128, 256, 512, 1024}

func scaled(src *image.NRGBA, size int) *image.NRGBA {
	dst := image.NewNRGBA(image.Rect(0, 0, size, size))
	for y := 0; y < size; y++ {
		for x := 0; x < size; x++ {
			sx := x * src.Bounds().Dx() / size
			sy := y * src.Bounds().Dy() / size
			dst.SetNRGBA(x, y, src.NRGBAAt(sx, sy))
		}
	}
	return dst
}

func pngBytes(img image.Image) []byte {
	var data bytes.Buffer
	encoder := png.Encoder{CompressionLevel: png.BestCompression}
	if err := encoder.Encode(&data, img); err != nil {
		panic(err)
	}
	return data.Bytes()
}

func main() {
	f, err := os.Open("assets/branding/source/Sudokura05.png")
	if err != nil {
		panic(err)
	}
	decoded, err := png.Decode(f)
	f.Close()
	if err != nil {
		panic(err)
	}
	b := decoded.Bounds()
	minX, minY, maxX, maxY := b.Max.X, b.Max.Y, b.Min.X, b.Min.Y
	clean := image.NewNRGBA(image.Rect(0, 0, b.Dx(), b.Dy()))
	for y := b.Min.Y; y < b.Max.Y; y++ {
		for x := b.Min.X; x < b.Max.X; x++ {
			c := color.NRGBAModel.Convert(decoded.At(x, y)).(color.NRGBA)
			if c.A <= 8 {
				c = color.NRGBA{}
			}
			clean.SetNRGBA(x-b.Min.X, y-b.Min.Y, c)
			if c.A > 0 {
				if x < minX {
					minX = x
				}
				if y < minY {
					minY = y
				}
				if x+1 > maxX {
					maxX = x + 1
				}
				if y+1 > maxY {
					maxY = y + 1
				}
			}
		}
	}
	if maxX <= minX || maxY <= minY {
		panic("source has no visible pixels")
	}
	crop := clean.SubImage(image.Rect(minX-b.Min.X, minY-b.Min.Y, maxX-b.Min.X, maxY-b.Min.Y)).(*image.NRGBA)
	content := crop.Bounds().Dx()
	if crop.Bounds().Dy() > content {
		content = crop.Bounds().Dy()
	}
	canvas := image.NewNRGBA(image.Rect(0, 0, content*5/4, content*5/4))
	draw.Draw(canvas, image.Rect((canvas.Bounds().Dx()-crop.Bounds().Dx())/2, (canvas.Bounds().Dy()-crop.Bounds().Dy())/2, (canvas.Bounds().Dx()+crop.Bounds().Dx())/2, (canvas.Bounds().Dy()+crop.Bounds().Dy())/2), crop, crop.Bounds().Min, draw.Src)
	if err := os.MkdirAll("assets/generated", 0755); err != nil {
		panic(err)
	}
	data := make([][]byte, len(sizes))
	images := make([]*image.NRGBA, len(sizes))
	for i, n := range sizes {
		images[i] = scaled(canvas, n)
		data[i] = pngBytes(images[i])
		os.WriteFile(fmt.Sprintf("assets/generated/sudokura-%d.png", n), data[i], 0644)
	}
	var ico bytes.Buffer
	binary.Write(&ico, binary.LittleEndian, uint16(0))
	binary.Write(&ico, binary.LittleEndian, uint16(1))
	binary.Write(&ico, binary.LittleEndian, uint16(6))
	offset := 6 + 6*16
	for i := 0; i < 6; i++ {
		n := sizes[i]
		w, h := byte(n), byte(n)
		if n == 256 {
			w = 0
			h = 0
		}
		ico.WriteByte(w)
		ico.WriteByte(h)
		ico.Write([]byte{0, 0})
		binary.Write(&ico, binary.LittleEndian, uint16(1))
		binary.Write(&ico, binary.LittleEndian, uint16(32))
		binary.Write(&ico, binary.LittleEndian, uint32(len(data[i])))
		binary.Write(&ico, binary.LittleEndian, uint32(offset))
		offset += len(data[i])
	}
	for i := 0; i < 6; i++ {
		ico.Write(data[i])
	}
	os.WriteFile("assets/generated/sudokura.ico", ico.Bytes(), 0644)
	types := []string{"icp4", "icp5", "icp6", "ic07", "ic08", "ic09", "ic10"}
	iconIndexes := []int{0, 1, 3, 4, 5, 6, 7}
	var body bytes.Buffer
	for i, t := range types {
		d := data[iconIndexes[i]]
		body.WriteString(t)
		binary.Write(&body, binary.BigEndian, uint32(len(d)+8))
		body.Write(d)
	}
	var icns bytes.Buffer
	icns.WriteString("icns")
	binary.Write(&icns, binary.BigEndian, uint32(body.Len()+8))
	icns.Write(body.Bytes())
	os.WriteFile("assets/generated/sudokura.icns", icns.Bytes(), 0644)
	var c bytes.Buffer
	c.WriteString("#include \"window_icon.h\"\nconst unsigned int sudokura_icon_width=128,sudokura_icon_height=128;\nconst unsigned char sudokura_icon_rgba[]={")
	for i, v := range images[4].Pix {
		if i%20 == 0 {
			c.WriteByte('\n')
		}
		fmt.Fprintf(&c, "%d,", v)
	}
	c.WriteString("\n};\n")
	os.WriteFile("assets/generated/window_icon.c", c.Bytes(), 0644)
	os.WriteFile("assets/generated/window_icon.h", []byte("#ifndef WINDOW_ICON_H\n#define WINDOW_ICON_H\nextern const unsigned int sudokura_icon_width,sudokura_icon_height;\nextern const unsigned char sudokura_icon_rgba[];\n#endif\n"), 0644)
	files, _ := filepath.Glob("assets/generated/*")
	fmt.Printf("generated and validated %d icon resources\n", len(files))
}
