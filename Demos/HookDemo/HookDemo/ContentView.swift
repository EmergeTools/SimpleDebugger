//
//  ContentView.swift
//  HookDemo
//
//  Created by Noah Martin on 9/12/25.
//

import SwiftUI

struct ContentView: View {
  
  @State var color: UIColor = UIColor.blue
  @State var result: String = ""
    var body: some View {
        VStack {
            Image(systemName: "globe")
                .imageScale(.large)
                .foregroundStyle(.tint)
          
          Button("Install hook \(result)") {
            DispatchQueue.global(qos: .default).async {
              result = "\(installHook())"
              DispatchQueue.main.async {
                color = makeColor()
              }
            }
          }
            Text("Hello, world!")
              .foregroundStyle(Color(uiColor: color))
        }
        .padding()
    }
}

#Preview {
    ContentView()
}
