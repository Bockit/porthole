// find_window — print on-screen windows matching a title or owner.
//
// Build (one-time): swiftc -O tests/find_window.swift -o tests/find_window
//
// Usage:
//   tests/find_window                          # list all on-screen windows
//   tests/find_window --title Steam            # title substring match (case-insensitive)
//   tests/find_window --owner wine             # owner-name substring match (case-insensitive)
//   tests/find_window --title Steam --bounds   # print "x,y,w,h" of first match (for screencapture -R)
//
// Uses CGWindowListCopyWindowInfo (read-only, no AX permission required).

import CoreGraphics
import Foundation

struct Args {
    var titlePattern: String? = nil
    var ownerPattern: String? = nil
    var boundsOnly = false
    var idOnly = false
}

func parseArgs() -> Args {
    var a = Args()
    var it = CommandLine.arguments.dropFirst().makeIterator()
    while let arg = it.next() {
        switch arg {
        case "--title":  a.titlePattern = it.next()?.lowercased()
        case "--owner":  a.ownerPattern = it.next()?.lowercased()
        case "--bounds": a.boundsOnly = true
        case "--id":     a.idOnly = true
        default:
            FileHandle.standardError.write(Data("unknown arg: \(arg)\n".utf8))
            exit(2)
        }
    }
    return a
}

let args = parseArgs()

guard let list = CGWindowListCopyWindowInfo(
    [.optionOnScreenOnly, .excludeDesktopElements],
    kCGNullWindowID
) as? [[String: Any]] else {
    FileHandle.standardError.write(Data("CGWindowListCopyWindowInfo failed\n".utf8))
    exit(3)
}

struct Hit {
    let id: Int
    let owner: String
    let title: String
    let x: Int; let y: Int; let w: Int; let h: Int
    let layer: Int
}

var hits: [Hit] = []
for w in list {
    let owner = (w[kCGWindowOwnerName as String] as? String) ?? ""
    let title = (w[kCGWindowName as String] as? String) ?? ""
    let id = (w[kCGWindowNumber as String] as? Int) ?? 0
    let layer = (w[kCGWindowLayer as String] as? Int) ?? 0
    guard let b = w[kCGWindowBounds as String] as? [String: Any] else { continue }
    let x = Int((b["X"] as? CGFloat) ?? 0)
    let y = Int((b["Y"] as? CGFloat) ?? 0)
    let width = Int((b["Width"] as? CGFloat) ?? 0)
    let height = Int((b["Height"] as? CGFloat) ?? 0)
    if width < 10 || height < 10 { continue }

    if let p = args.titlePattern, !title.lowercased().contains(p) { continue }
    if let p = args.ownerPattern, !owner.lowercased().contains(p) { continue }

    hits.append(Hit(id: id, owner: owner, title: title,
                    x: x, y: y, w: width, h: height, layer: layer))
}

if args.idOnly {
    if let h = hits.first {
        print("\(h.id)")
        exit(0)
    } else {
        exit(1)
    }
}

if args.boundsOnly {
    if let h = hits.first {
        print("\(h.x),\(h.y),\(h.w),\(h.h)")
        exit(0)
    } else {
        exit(1)
    }
}

func pad(_ s: String, _ n: Int) -> String {
    let t = String(s.prefix(n))
    return t + String(repeating: " ", count: max(0, n - t.count))
}

for h in hits {
    let id   = pad(String(h.id), 6)
    let lay  = pad("L\(h.layer)", 4)
    let own  = pad(h.owner, 22)
    let ttl  = pad(h.title, 50)
    let geom = "\(h.x),\(h.y) \(h.w)x\(h.h)"
    print("\(id) \(lay) \(own) \(ttl) \(geom)")
}
