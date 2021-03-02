#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)
library(scales)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
credit <- args[3]
data<-read.csv(file)
data$Prob <- data$Prob * 100
data$Topo <- factor(data$Topo, levels=c("sw", "sf", "lnd"), ordered=T)
newdata <- data[ which(data$CreditType==credit),]
range_data<-read.csv("size_range_data")
range_data$level <- as.factor(range_data$level)


label_list <- c("PS" = "Primal Dual Approach",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "LND" = "LND",
                 "DCTCP" = "Diffindo",
                 "DCTCP_qdelay" = "Spider")

break_list <- c("PS", "SP", "LR", "WF", "LND", "DCTCP", "DCTCP_qdelay")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP" = "#a65628",
                "DCTCP_qdelay" = "#0c2c84")

shape_list <- c(
                "PS" = 18,
                "LND" = 8,
                "LR" = 7,
                "SP" = 4,
                "WF" = 15,
                "DCTCP" = 17,
                "DCTCP_qdelay" = 19)

topo_mapping <- c(
                   "sw" = "Small World",
                   "sf" = "Scale Free",
                   "lnd" = "Lightning Network")

credit_mapping <- c(
                   "uniform" = "Equal Channel Sizes",
                   "lnd" = "Lightning Sampled Sizes")


labeller_func <- function(variable,value){
        return(topo_mapping[value])
}

# add a line plot
succ_ratio_plot <- ggplot(newdata) +  
        facet_grid(. ~ Topo, labeller=labeller_func) +
        geom_rect(data=range_data, 
                 aes_string(xmin="start", xmax="end", ymin=-Inf, ymax=Inf, fill="level"), 
                  alpha=0.5,
                  show.legend=FALSE) +

        geom_line(data=newdata, aes_string(x="Point",y="Prob", colour="Scheme"), size=1) +
        geom_point(data=newdata, aes_string(x="Point",y="Prob", colour="Scheme", shape="Scheme"), size=2) +
        
        scale_x_continuous(expand=c(0,0), trans="log2", breaks=c(5,10,15,25,41,82,164,3930)) +
        coord_cartesian(ylim=c(0,100),xlim=c(2,5000)) +
        
        labs(x="Transaction Size (€)", y="Success Ratio (%)") +

        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        scale_fill_brewer(type = "seq", palette = "Blues", direction = 1,
                            aesthetics = "fill") +
        theme_minimal(base_size=12) + 
        theme(legend.text=element_text(size=rel(1.2)), 
              legend.key.size=unit(12,"points"), 
              legend.box.margin=margin(-10,-10,-10,-10), legend.margin=margin(c(5,5,1,5)),
              legend.position="top") +
        theme(axis.text.x = element_text(angle = 70, hjust = 1, size=12),axis.title=element_text(size=15)) +
        theme(strip.text.x = element_text(size=14), strip.text.y = element_text(size=10))

ggsave(plot_filename, width=12, height=3)

