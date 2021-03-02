#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
args <- commandArgs(trailingOnly=TRUE)
file <- args[1] 
plot_filename <- args[2]
data<-read.csv(file)
data$Topo <- factor(data$Topo, levels=c("Small World", "Scale Free", "Lightning Topology"), ordered=T)
data$PathType <- factor(data$PathType, levels=c("widest", "shortest", "kspYen", "heuristic", "oblivious"), ordered=T)
rel_schemes <- data[ which(data$Scheme=='DCTCP_qdelay'),]

label_list <- c("widest" = "Edge-disjoint Widest",
                 "kspYen" = "Shortest (Yen's)",
                 "shortest" = "Edge-disjoint Shortest  ",
                 "oblivious" = "Oblivious ",
                 "heuristic" = "Heuristic ")

color_list <- c(
                "widest" = "#045a8d",
                "heuristic" = "#fc9272",
                "kspYen" = "#7570b3",
                "oblivious" = "#8dd3c7",
                "shortest" = "#e6ab02")

pos <- "top"
height <- 3

# add a line plot
succ_ratio_plot <- ggplot(rel_schemes, aes_string(x="Topo", y="SuccRatio", fill="PathType")) +
        geom_bar(stat="identity", position=position_dodge(), colour="black") +
        geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"), width=.3, 
                      position=position_dodge(.9)) +
        labs(x="Topology", y="Success Ratio (%)") +
        scale_fill_manual(values=color_list,
                          labels=label_list)+
        theme_minimal(base_size=10) +
        theme(legend.text=element_text(size=rel(0.85)), legend.key.size=unit(10,"points")) +  
        theme(legend.position=pos, legend.margin=margin(c(1,7,1,0)))+
        theme(legend.title=element_blank())+
        theme(plot.margin=margin(c(0.5,0,0,0.5))) +
        guides(fill=guide_legend(nrow=2, row=T))

ggsave(plot_filename, width=4, height=1.5)
